const std = @import("../std.zig");
const math = std.math;

/// Returns whether x is negative or negative 0.
pub fn signbit(x: anytype) bool {
    const T = @TypeOf(x);
    const TBits = std.meta.Int(.unsigned, @typeInfo(T).Float.bits);
    return @as(TBits, @bitCast(x)) >> (@bitSizeOf(T) - 1) != 0;
}
