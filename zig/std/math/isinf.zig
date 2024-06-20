const std = @import("../std.zig");
const math = std.math;

/// Returns whether x is an infinity, ignoring sign.
pub inline fn isInf(x: anytype) bool {
    const T = @TypeOf(x);
    const TBits = std.meta.Int(.unsigned, @typeInfo(T).Float.bits);
    const remove_sign = ~@as(TBits, 0) >> 1;
    return @as(TBits, @bitCast(x)) & remove_sign == @as(TBits, @bitCast(math.inf(T)));
}

/// Returns whether x is an infinity with a positive sign.
pub inline fn isPositiveInf(x: anytype) bool {
    return x == math.inf(@TypeOf(x));
}

/// Returns whether x is an infinity with a negative sign.
pub inline fn isNegativeInf(x: anytype) bool {
    return x == -math.inf(@TypeOf(x));
}
