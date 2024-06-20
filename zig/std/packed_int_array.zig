//! A set of array and slice types that bit-pack integer elements. A normal [12]u3
//! takes up 12 bytes of memory since u3's alignment is 1. PackedArray(u3, 12) only
//! takes up 4 bytes of memory.

const std = @import("std");
const builtin = @import("lightos/builtin.zig");
const debug = std.debug;
const native_endian = builtin.target.cpu.arch.endian();
const Endian = std.builtin.Endian;

/// Provides a set of functions for reading and writing packed integers from a
/// slice of bytes.
pub fn PackedIntIo(comptime Int: type, comptime endian: Endian) type {
    // The general technique employed here is to cast bytes in the array to a container
    // integer (having bits % 8 == 0) large enough to contain the number of bits we want,
    // then we can retrieve or store the new value with a relative minimum of masking
    // and shifting. In this worst case, this means that we'll need an integer that's
    // actually 1 byte larger than the minimum required to store the bits, because it
    // is possible that the bits start at the end of the first byte, continue through
    // zero or more, then end in the beginning of the last. But, if we try to access
    // a value in the very last byte of memory with that integer size, that extra byte
    // will be out of bounds. Depending on the circumstances of the memory, that might
    // mean the OS fatally kills the program. Thus, we use a larger container (MaxIo)
    // most of the time, but a smaller container (MinIo) when touching the last byte
    // of the memory.
    const int_bits = @bitSizeOf(Int);

    // In the best case, this is the number of bytes we need to touch
    // to read or write a value, as bits.
    const min_io_bits = ((int_bits + 7) / 8) * 8;

    // In the worst case, this is the number of bytes we need to touch
    // to read or write a value, as bits. To calculate for int_bits > 1,
    // set aside 2 bits to touch the first and last bytes, then divide
    // by 8 to see how many bytes can be filled up in between.
    const max_io_bits = switch (int_bits) {
        0 => 0,
        1 => 8,
        else => ((int_bits - 2) / 8 + 2) * 8,
    };

    // We bitcast the desired Int type to an unsigned version of itself
    // to avoid issues with shifting signed ints.
    const UnInt = std.meta.Int(.unsigned, int_bits);

    // The maximum container int type
    const MinIo = std.meta.Int(.unsigned, min_io_bits);

    // The minimum container int type
    const MaxIo = std.meta.Int(.unsigned, max_io_bits);

    return struct {
        /// Retrieves the integer at `index` from the packed data beginning at `bit_offset`
        /// within `bytes`.
        pub fn get(bytes: []const u8, index: usize, bit_offset: u7) Int {
            if (int_bits == 0) return 0;

            const bit_index = (index * int_bits) + bit_offset;
            const max_end_byte = (bit_index + max_io_bits) / 8;

            //using the larger container size will potentially read out of bounds
            if (max_end_byte > bytes.len) return getBits(bytes, MinIo, bit_index);
            return getBits(bytes, MaxIo, bit_index);
        }

        fn getBits(bytes: []const u8, comptime Container: type, bit_index: usize) Int {
            const container_bits = @bitSizeOf(Container);
            const Shift = std.math.Log2Int(Container);

            const start_byte = bit_index / 8;
            const head_keep_bits = bit_index - (start_byte * 8);
            const tail_keep_bits = container_bits - (int_bits + head_keep_bits);

            //read bytes as container
            const value_ptr = @as(*align(1) const Container, @ptrCast(&bytes[start_byte]));
            var value = value_ptr.*;

            if (endian != native_endian) value = @byteSwap(value);

            switch (endian) {
                .big => {
                    value <<= @as(Shift, @intCast(head_keep_bits));
                    value >>= @as(Shift, @intCast(head_keep_bits));
                    value >>= @as(Shift, @intCast(tail_keep_bits));
                },
                .little => {
                    value <<= @as(Shift, @intCast(tail_keep_bits));
                    value >>= @as(Shift, @intCast(tail_keep_bits));
                    value >>= @as(Shift, @intCast(head_keep_bits));
                },
            }

            return @as(Int, @bitCast(@as(UnInt, @truncate(value))));
        }

        /// Sets the integer at `index` to `val` within the packed data beginning
        /// at `bit_offset` into `bytes`.
        pub fn set(bytes: []u8, index: usize, bit_offset: u3, int: Int) void {
            if (int_bits == 0) return;

            const bit_index = (index * int_bits) + bit_offset;
            const max_end_byte = (bit_index + max_io_bits) / 8;

            //using the larger container size will potentially write out of bounds
            if (max_end_byte > bytes.len) return setBits(bytes, MinIo, bit_index, int);
            setBits(bytes, MaxIo, bit_index, int);
        }

        fn setBits(bytes: []u8, comptime Container: type, bit_index: usize, int: Int) void {
            const container_bits = @bitSizeOf(Container);
            const Shift = std.math.Log2Int(Container);

            const start_byte = bit_index / 8;
            const head_keep_bits = bit_index - (start_byte * 8);
            const tail_keep_bits = container_bits - (int_bits + head_keep_bits);
            const keep_shift = switch (endian) {
                .big => @as(Shift, @intCast(tail_keep_bits)),
                .little => @as(Shift, @intCast(head_keep_bits)),
            };

            //position the bits where they need to be in the container
            const value = @as(Container, @intCast(@as(UnInt, @bitCast(int)))) << keep_shift;

            //read existing bytes
            const target_ptr = @as(*align(1) Container, @ptrCast(&bytes[start_byte]));
            var target = target_ptr.*;

            if (endian != native_endian) target = @byteSwap(target);

            //zero the bits we want to replace in the existing bytes
            const inv_mask = @as(Container, @intCast(std.math.maxInt(UnInt))) << keep_shift;
            const mask = ~inv_mask;
            target &= mask;

            //merge the new value
            target |= value;

            if (endian != native_endian) target = @byteSwap(target);

            //save it back
            target_ptr.* = target;
        }

        /// Provides a PackedIntSlice of the packed integers in `bytes` (which begins at `bit_offset`)
        /// from the element specified by `start` to the element specified by `end`.
        pub fn slice(bytes: []u8, bit_offset: u3, start: usize, end: usize) PackedIntSliceEndian(Int, endian) {
            debug.assert(end >= start);

            const length = end - start;
            const bit_index = (start * int_bits) + bit_offset;
            const start_byte = bit_index / 8;
            const end_byte = (bit_index + (length * int_bits) + 7) / 8;
            const new_bytes = bytes[start_byte..end_byte];

            if (length == 0) return PackedIntSliceEndian(Int, endian).init(new_bytes[0..0], 0);

            var new_slice = PackedIntSliceEndian(Int, endian).init(new_bytes, length);
            new_slice.bit_offset = @as(u3, @intCast((bit_index - (start_byte * 8))));
            return new_slice;
        }

        /// Recasts a packed slice to a version with elements of type `NewInt` and endianness `new_endian`.
        /// Slice will begin at `bit_offset` within `bytes` and the new length will be automatically
        /// calculated from `old_len` using the sizes of the current integer type and `NewInt`.
        pub fn sliceCast(bytes: []u8, comptime NewInt: type, comptime new_endian: Endian, bit_offset: u3, old_len: usize) PackedIntSliceEndian(NewInt, new_endian) {
            const new_int_bits = @bitSizeOf(NewInt);
            const New = PackedIntSliceEndian(NewInt, new_endian);

            const total_bits = (old_len * int_bits);
            const new_int_count = total_bits / new_int_bits;

            debug.assert(total_bits == new_int_count * new_int_bits);

            var new = New.init(bytes, new_int_count);
            new.bit_offset = bit_offset;

            return new;
        }
    };
}

/// Creates a bit-packed array of `Int`. Non-byte-multiple integers
/// will take up less memory in PackedIntArray than in a normal array.
/// Elements are packed using native endianness and without storing any
/// meta data. PackedArray(i3, 8) will occupy exactly 3 bytes
/// of memory.
pub fn PackedIntArray(comptime Int: type, comptime int_count: usize) type {
    return PackedIntArrayEndian(Int, native_endian, int_count);
}

/// Creates a bit-packed array of `Int` with bit order specified by `endian`.
/// Non-byte-multiple integers will take up less memory in PackedIntArrayEndian
/// than in a normal array. Elements are packed without storing any meta data.
/// PackedIntArrayEndian(i3, 8) will occupy exactly 3 bytes of memory.
pub fn PackedIntArrayEndian(comptime Int: type, comptime endian: Endian, comptime int_count: usize) type {
    const int_bits = @bitSizeOf(Int);
    const total_bits = int_bits * int_count;
    const total_bytes = (total_bits + 7) / 8;

    const Io = PackedIntIo(Int, endian);

    return struct {
        const Self = @This();

        /// The byte buffer containing the packed data.
        bytes: [total_bytes]u8,
        /// The number of elements in the packed array.
        comptime len: usize = int_count,

        /// The integer type of the packed array.
        pub const Child = Int;

        /// Initialize a packed array using an unpacked array
        /// or, more likely, an array literal.
        pub fn init(ints: [int_count]Int) Self {
            var self = @as(Self, undefined);
            for (ints, 0..) |int, i| self.set(i, int);
            return self;
        }

        /// Initialize all entries of a packed array to the same value.
        pub fn initAllTo(int: Int) Self {
            // TODO: use `var self = @as(Self, undefined);` https://github.com/ziglang/zig/issues/7635
            var self = Self{ .bytes = [_]u8{0} ** total_bytes, .len = int_count };
            self.setAll(int);
            return self;
        }

        /// Return the integer stored at `index`.
        pub fn get(self: Self, index: usize) Int {
            debug.assert(index < int_count);
            return Io.get(&self.bytes, index, 0);
        }

        ///Copy the value of `int` into the array at `index`.
        pub fn set(self: *Self, index: usize, int: Int) void {
            debug.assert(index < int_count);
            return Io.set(&self.bytes, index, 0, int);
        }

        /// Set all entries of a packed array to the value of `int`.
        pub fn setAll(self: *Self, int: Int) void {
            var i: usize = 0;
            while (i < int_count) : (i += 1) {
                self.set(i, int);
            }
        }

        /// Create a PackedIntSlice of the array from `start` to `end`.
        pub fn slice(self: *Self, start: usize, end: usize) PackedIntSliceEndian(Int, endian) {
            debug.assert(start < int_count);
            debug.assert(end <= int_count);
            return Io.slice(&self.bytes, 0, start, end);
        }

        /// Create a PackedIntSlice of the array using `NewInt` as the integer type.
        /// `NewInt`'s bit width must fit evenly within the array's `Int`'s total bits.
        pub fn sliceCast(self: *Self, comptime NewInt: type) PackedIntSlice(NewInt) {
            return self.sliceCastEndian(NewInt, endian);
        }

        /// Create a PackedIntSliceEndian of the array using `NewInt` as the integer type
        /// and `new_endian` as the new endianness. `NewInt`'s bit width must fit evenly
        /// within the array's `Int`'s total bits.
        pub fn sliceCastEndian(self: *Self, comptime NewInt: type, comptime new_endian: Endian) PackedIntSliceEndian(NewInt, new_endian) {
            return Io.sliceCast(&self.bytes, NewInt, new_endian, 0, int_count);
        }
    };
}

/// A type representing a sub range of a PackedIntArray.
pub fn PackedIntSlice(comptime Int: type) type {
    return PackedIntSliceEndian(Int, native_endian);
}

/// A type representing a sub range of a PackedIntArrayEndian.
pub fn PackedIntSliceEndian(comptime Int: type, comptime endian: Endian) type {
    const int_bits = @bitSizeOf(Int);
    const Io = PackedIntIo(Int, endian);

    return struct {
        const Self = @This();

        bytes: []u8,
        bit_offset: u3,
        len: usize,

        /// The integer type of the packed slice.
        pub const Child = Int;

        /// Calculates the number of bytes required to store a desired count
        /// of `Int`s.
        pub fn bytesRequired(int_count: usize) usize {
            const total_bits = int_bits * int_count;
            const total_bytes = (total_bits + 7) / 8;
            return total_bytes;
        }

        /// Initialize a packed slice using the memory at `bytes`, with `int_count`
        /// elements. `bytes` must be large enough to accommodate the requested
        /// count.
        pub fn init(bytes: []u8, int_count: usize) Self {
            debug.assert(bytes.len >= bytesRequired(int_count));

            return Self{
                .bytes = bytes,
                .len = int_count,
                .bit_offset = 0,
            };
        }

        /// Return the integer stored at `index`.
        pub fn get(self: Self, index: usize) Int {
            debug.assert(index < self.len);
            return Io.get(self.bytes, index, self.bit_offset);
        }

        /// Copy `int` into the slice at `index`.
        pub fn set(self: *Self, index: usize, int: Int) void {
            debug.assert(index < self.len);
            return Io.set(self.bytes, index, self.bit_offset, int);
        }

        /// Create a PackedIntSlice of this slice from `start` to `end`.
        pub fn slice(self: Self, start: usize, end: usize) PackedIntSliceEndian(Int, endian) {
            debug.assert(start < self.len);
            debug.assert(end <= self.len);
            return Io.slice(self.bytes, self.bit_offset, start, end);
        }

        /// Create a PackedIntSlice of the sclice using `NewInt` as the integer type.
        /// `NewInt`'s bit width must fit evenly within the slice's `Int`'s total bits.
        pub fn sliceCast(self: Self, comptime NewInt: type) PackedIntSliceEndian(NewInt, endian) {
            return self.sliceCastEndian(NewInt, endian);
        }

        /// Create a PackedIntSliceEndian of the slice using `NewInt` as the integer type
        /// and `new_endian` as the new endianness. `NewInt`'s bit width must fit evenly
        /// within the slice's `Int`'s total bits.
        pub fn sliceCastEndian(self: Self, comptime NewInt: type, comptime new_endian: Endian) PackedIntSliceEndian(NewInt, new_endian) {
            return Io.sliceCast(self.bytes, NewInt, new_endian, self.bit_offset, self.len);
        }
    };
}
