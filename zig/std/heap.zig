const std = @import("std.zig");
const builtin = @import("builtin");
const lightos_mem = @import("lightos/mem.zig");
const assert = std.assert;
const mem = std.mem;
const Allocator = std.mem.Allocator;

/// Export the arena allocator, just cuz we need it somewhere lmao
pub const ArenaAllocator = @import("mem/arena_allocator.zig").ArenaAllocator;

/// Our glorious page allocator, fueled directly
/// By our beautiful kernel, which hopefully didn't
/// Fuck up linking to kernel symbols \_(X_X)_/
pub const page_allocator = Allocator{
    .ptr = undefined,
    .vtable = lightos_mem.vtable,
};

fn sliceContainsPtr(container: []u8, ptr: [*]u8) bool {
    return @intFromPtr(ptr) >= @intFromPtr(container.ptr) and
        @intFromPtr(ptr) < (@intFromPtr(container.ptr) + container.len);
}

fn sliceContainsSlice(container: []u8, slice: []u8) bool {
    return @intFromPtr(slice.ptr) >= @intFromPtr(container.ptr) and
        (@intFromPtr(slice.ptr) + slice.len) <= (@intFromPtr(container.ptr) + container.len);
}

pub const FixedBufferAllocator = struct {
    end_index: usize,
    buffer: []u8,

    pub fn init(buffer: []u8) FixedBufferAllocator {
        return FixedBufferAllocator{
            .buffer = buffer,
            .end_index = 0,
        };
    }

    /// *WARNING* using this at the same time as the interface returned by `threadSafeAllocator` is not thread safe
    pub fn allocator(self: *FixedBufferAllocator) Allocator {
        return .{
            .ptr = self,
            .vtable = &.{
                .alloc = alloc,
                .resize = resize,
                .free = free,
            },
        };
    }

    /// Provides a lock free thread safe `Allocator` interface to the underlying `FixedBufferAllocator`
    /// *WARNING* using this at the same time as the interface returned by `allocator` is not thread safe
    pub fn threadSafeAllocator(self: *FixedBufferAllocator) Allocator {
        return .{
            .ptr = self,
            .vtable = &.{
                .alloc = threadSafeAlloc,
                .resize = Allocator.noResize,
                .free = Allocator.noFree,
            },
        };
    }

    pub fn ownsPtr(self: *FixedBufferAllocator, ptr: [*]u8) bool {
        return sliceContainsPtr(self.buffer, ptr);
    }

    pub fn ownsSlice(self: *FixedBufferAllocator, slice: []u8) bool {
        return sliceContainsSlice(self.buffer, slice);
    }

    /// NOTE: this will not work in all cases, if the last allocation had an adjusted_index
    ///       then we won't be able to determine what the last allocation was.  This is because
    ///       the alignForward operation done in alloc is not reversible.
    pub fn isLastAllocation(self: *FixedBufferAllocator, buf: []u8) bool {
        return buf.ptr + buf.len == self.buffer.ptr + self.end_index;
    }

    fn alloc(ctx: *anyopaque, n: usize, log2_ptr_align: u8, ra: usize) ?[*]u8 {
        const self: *FixedBufferAllocator = @ptrCast(@alignCast(ctx));
        _ = ra;
        const ptr_align = @as(usize, 1) << @as(Allocator.Log2Align, @intCast(log2_ptr_align));
        const adjust_off = mem.alignPointerOffset(self.buffer.ptr + self.end_index, ptr_align) orelse return null;
        const adjusted_index = self.end_index + adjust_off;
        const new_end_index = adjusted_index + n;
        if (new_end_index > self.buffer.len) return null;
        self.end_index = new_end_index;
        return self.buffer.ptr + adjusted_index;
    }

    fn resize(
        ctx: *anyopaque,
        buf: []u8,
        log2_buf_align: u8,
        new_size: usize,
        return_address: usize,
    ) bool {
        const self: *FixedBufferAllocator = @ptrCast(@alignCast(ctx));
        _ = log2_buf_align;
        _ = return_address;
        assert(@inComptime() or self.ownsSlice(buf));

        if (!self.isLastAllocation(buf)) {
            if (new_size > buf.len) return false;
            return true;
        }

        if (new_size <= buf.len) {
            const sub = buf.len - new_size;
            self.end_index -= sub;
            return true;
        }

        const add = new_size - buf.len;
        if (add + self.end_index > self.buffer.len) return false;

        self.end_index += add;
        return true;
    }

    fn free(
        ctx: *anyopaque,
        buf: []u8,
        log2_buf_align: u8,
        return_address: usize,
    ) void {
        const self: *FixedBufferAllocator = @ptrCast(@alignCast(ctx));
        _ = log2_buf_align;
        _ = return_address;
        assert(@inComptime() or self.ownsSlice(buf));

        if (self.isLastAllocation(buf)) {
            self.end_index -= buf.len;
        }
    }

    fn threadSafeAlloc(ctx: *anyopaque, n: usize, log2_ptr_align: u8, ra: usize) ?[*]u8 {
        const self: *FixedBufferAllocator = @ptrCast(@alignCast(ctx));
        _ = ra;
        const ptr_align = @as(usize, 1) << @as(Allocator.Log2Align, @intCast(log2_ptr_align));
        var end_index = @atomicLoad(usize, &self.end_index, .SeqCst);
        while (true) {
            const adjust_off = mem.alignPointerOffset(self.buffer.ptr + end_index, ptr_align) orelse return null;
            const adjusted_index = end_index + adjust_off;
            const new_end_index = adjusted_index + n;
            if (new_end_index > self.buffer.len) return null;
            end_index = @cmpxchgWeak(usize, &self.end_index, end_index, new_end_index, .SeqCst, .SeqCst) orelse
                return self.buffer[adjusted_index..new_end_index].ptr;
        }
    }

    pub fn reset(self: *FixedBufferAllocator) void {
        self.end_index = 0;
    }
};
