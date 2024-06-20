const std = @import("../std.zig");
const math = std.math;

/// Returns whether x is neither zero, subnormal, infinity, or NaN.
pub fn isNormal(x: anytype) bool {
    const T = @TypeOf(x);
    const TBits = std.meta.Int(.unsigned, @typeInfo(T).Float.bits);

    const increment_exp = 1 << math.floatMantissaBits(T);
    const remove_sign = ~@as(TBits, 0) >> 1;

    // We add 1 to the exponent, and if it overflows to 0 or becomes 1,
    // then it was all zeroes (subnormal) or all ones (special, inf/nan).
    // The sign bit is removed because all ones would overflow into it.
    // For f80, even though it has an explicit integer part stored,
    // the exponent effectively takes priority if mismatching.
    const value = @as(TBits, @bitCast(x)) +% increment_exp;
    return value & remove_sign >= (increment_exp << 1);
}
