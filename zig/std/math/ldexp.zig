const std = @import("std");
const math = std.math;
const Log2Int = std.math.Log2Int;
const assert = std.assert;

/// Returns x * 2^n.
pub fn ldexp(x: anytype, n: i32) @TypeOf(x) {
    const T = @TypeOf(x);
    const TBits = std.meta.Int(.unsigned, @typeInfo(T).Float.bits);

    const exponent_bits = math.floatExponentBits(T);
    const mantissa_bits = math.floatMantissaBits(T);
    const fractional_bits = math.floatFractionalBits(T);

    const max_biased_exponent = 2 * math.floatExponentMax(T);
    const mantissa_mask = @as(TBits, (1 << mantissa_bits) - 1);

    const repr = @as(TBits, @bitCast(x));
    const sign_bit = repr & (1 << (exponent_bits + mantissa_bits));

    if (math.isNan(x) or !math.isFinite(x))
        return x;

    var exponent: i32 = @as(i32, @intCast((repr << 1) >> (mantissa_bits + 1)));
    if (exponent == 0)
        exponent += (@as(i32, exponent_bits) + @intFromBool(T == f80)) - @clz(repr << 1);

    if (n >= 0) {
        if (n > max_biased_exponent - exponent) {
            // Overflow. Return +/- inf
            return @as(T, @bitCast(@as(TBits, @bitCast(math.inf(T))) | sign_bit));
        } else if (exponent + n <= 0) {
            // Result is subnormal
            return @as(T, @bitCast((repr << @as(Log2Int(TBits), @intCast(n))) | sign_bit));
        } else if (exponent <= 0) {
            // Result is normal, but needs shifting
            var result = @as(TBits, @intCast(n + exponent)) << mantissa_bits;
            result |= (repr << @as(Log2Int(TBits), @intCast(1 - exponent))) & mantissa_mask;
            return @as(T, @bitCast(result | sign_bit));
        }

        // Result needs no shifting
        return @as(T, @bitCast(repr + (@as(TBits, @intCast(n)) << mantissa_bits)));
    } else {
        if (n <= -exponent) {
            if (n < -(mantissa_bits + exponent))
                return @as(T, @bitCast(sign_bit)); // Severe underflow. Return +/- 0

            // Result underflowed, we need to shift and round
            const shift = @as(Log2Int(TBits), @intCast(@min(-n, -(exponent + n) + 1)));
            const exact_tie: bool = @ctz(repr) == shift - 1;
            var result = repr & mantissa_mask;

            if (T != f80) // Include integer bit
                result |= @as(TBits, @intFromBool(exponent > 0)) << fractional_bits;
            result = @as(TBits, @intCast((result >> (shift - 1))));

            // Round result, including round-to-even for exact ties
            result = ((result + 1) >> 1) & ~@as(TBits, @intFromBool(exact_tie));
            return @as(T, @bitCast(result | sign_bit));
        }

        // Result is exact, and needs no shifting
        return @as(T, @bitCast(repr - (@as(TBits, @intCast(-n)) << mantissa_bits)));
    }
}
