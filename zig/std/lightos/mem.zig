const std = @import("std");
const abi = std.lightos_abi;

const Allocator = std.mem.Allocator;

/// VTable struct to comply with zigs weird allocator shenanigans
pub const vtable = Allocator.VTable{
    .alloc = __lightos_kmem_alloc,
    .free = __lightos_kmem_dealloc,
    .resize = null,
};

/// Lmao we litaraly have the entire memory space xDDDD
fn __lightos_kmem_resize(_: *anyopaque, _: []u8, _: u8, _: usize, _: usize) bool {
    return true;
}

fn __lightos_kmem_alloc(ctx: *anyopaque, len: usize, log2_align: u8, ra: usize) ?[*]u8 {
    _ = ctx;
    _ = len;
    _ = log2_align;
    _ = ra;
    return null;
}

fn __lightos_kmem_dealloc(ctx: *anyopaque, buf: []u8, log2_align: u8, ra: usize) ?[*]u8 {
    _ = ctx;
    _ = buf;
    _ = log2_align;
    _ = ra;
}

pub fn lightos_kmem_map(paddr: u64, vbase: u64, n_page: usize, kmem_flags: u32, page_flags: u32) i32 {
    // Any alignment issues should be foregone by the kernel
    const result = abi.kmem_map_range(null, vbase, paddr, n_page, kmem_flags, page_flags);

    if (result)
        return -abi.KERR_INVAL;

    return 0;
}

pub fn lightos_kmem_alloc(n_bytes: usize, kmem_flags: u32, page_flags: u32) ?u64 {
    var err: i32 = 0;
    var result: u64 = 0;

    err = abi.__kmem_kernel_alloc_range(&result, n_bytes, kmem_flags, page_flags);

    if (err)
        return null;

    return result;
}

pub fn lightos_kmem_dealloc(addr: u64, n_bytes: usize) i32 {
    var err: i32 = 0;

    err = abi.__kmem_kernel_dealloc(addr, n_bytes);

    if (err)
        return err;

    return 0;
}
