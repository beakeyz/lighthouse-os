const std = @import("../std.zig");
const math = std.math;

/// Returns whether x is positive zero.
pub inline fn isPositiveZero(x: anytype) bool {
    const T = @TypeOf(x);
    const bit_count = @typeInfo(T).Float.bits;
    const TBits = std.meta.Int(.unsigned, bit_count);
    return @as(TBits, @bitCast(x)) == @as(TBits, 0);
}

/// Returns whether x is negative zero.
pub inline fn isNegativeZero(x: anytype) bool {
    const T = @TypeOf(x);
    const bit_count = @typeInfo(T).Float.bits;
    const TBits = std.meta.Int(.unsigned, bit_count);
    return @as(TBits, @bitCast(x)) == @as(TBits, 1) << (bit_count - 1);
}
