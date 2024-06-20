const std = @import("../std.zig");
const math = std.math;

/// Returns whether x is a finite value.
pub fn isFinite(x: anytype) bool {
    const T = @TypeOf(x);
    const TBits = std.meta.Int(.unsigned, @typeInfo(T).Float.bits);
    const remove_sign = ~@as(TBits, 0) >> 1;
    return @as(TBits, @bitCast(x)) & remove_sign < @as(TBits, @bitCast(math.inf(T)));
}
