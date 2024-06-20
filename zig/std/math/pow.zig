// Ported from go, which is licensed under a BSD-3 license.
// https://golang.org/LICENSE
//
// https://golang.org/src/math/pow.go

const std = @import("../std.zig");
const math = std.math;

/// Returns x raised to the power of y (x^y).
///
/// Special Cases:
///  - pow(x, +-0)    = 1 for any x
///  - pow(1, y)      = 1 for any y
///  - pow(x, 1)      = x for any x
///  - pow(nan, y)    = nan
///  - pow(x, nan)    = nan
///  - pow(+-0, y)    = +-inf for y an odd integer < 0
///  - pow(+-0, -inf) = +inf
///  - pow(+-0, +inf) = +0
///  - pow(+-0, y)    = +inf for finite y < 0 and not an odd integer
///  - pow(+-0, y)    = +-0 for y an odd integer > 0
///  - pow(+-0, y)    = +0 for finite y > 0 and not an odd integer
///  - pow(-1, +-inf) = 1
///  - pow(x, +inf)   = +inf for |x| > 1
///  - pow(x, -inf)   = +0 for |x| > 1
///  - pow(x, +inf)   = +0 for |x| < 1
///  - pow(x, -inf)   = +inf for |x| < 1
///  - pow(+inf, y)   = +inf for y > 0
///  - pow(+inf, y)   = +0 for y < 0
///  - pow(-inf, y)   = pow(-0, -y)
///  - pow(x, y)      = nan for finite x < 0 and finite non-integer y
pub fn pow(comptime T: type, x: T, y: T) T {
    if (@typeInfo(T) == .Int) {
        return math.powi(T, x, y) catch unreachable;
    }

    if (T != f32 and T != f64) {
        @compileError("pow not implemented for " ++ @typeName(T));
    }

    // pow(x, +-0) = 1      for all x
    // pow(1, y) = 1        for all y
    if (y == 0 or x == 1) {
        return 1;
    }

    // pow(nan, y) = nan    for all y
    // pow(x, nan) = nan    for all x
    if (math.isNan(x) or math.isNan(y)) {
        return math.nan(T);
    }

    // pow(x, 1) = x        for all x
    if (y == 1) {
        return x;
    }

    if (x == 0) {
        if (y < 0) {
            // pow(+-0, y) = +- 0   for y an odd integer
            if (isOddInteger(y)) {
                return math.copysign(math.inf(T), x);
            }
            // pow(+-0, y) = +inf   for y an even integer
            else {
                return math.inf(T);
            }
        } else {
            if (isOddInteger(y)) {
                return x;
            } else {
                return 0;
            }
        }
    }

    if (math.isInf(y)) {
        // pow(-1, inf) = 1     for all x
        if (x == -1) {
            return 1.0;
        }
        // pow(x, +inf) = +0    for |x| < 1
        // pow(x, -inf) = +0    for |x| > 1
        else if ((@abs(x) < 1) == math.isPositiveInf(y)) {
            return 0;
        }
        // pow(x, -inf) = +inf  for |x| < 1
        // pow(x, +inf) = +inf  for |x| > 1
        else {
            return math.inf(T);
        }
    }

    if (math.isInf(x)) {
        if (math.isNegativeInf(x)) {
            return pow(T, 1 / x, -y);
        }
        // pow(+inf, y) = +0    for y < 0
        else if (y < 0) {
            return 0;
        }
        // pow(+inf, y) = +0    for y > 0
        else if (y > 0) {
            return math.inf(T);
        }
    }

    // special case sqrt
    if (y == 0.5) {
        return @sqrt(x);
    }

    if (y == -0.5) {
        return 1 / @sqrt(x);
    }

    const r1 = math.modf(@abs(y));
    var yi = r1.ipart;
    var yf = r1.fpart;

    if (yf != 0 and x < 0) {
        return math.nan(T);
    }
    if (yi >= 1 << (@typeInfo(T).Float.bits - 1)) {
        return @exp(y * @log(x));
    }

    // a = a1 * 2^ae
    var a1: T = 1.0;
    var ae: i32 = 0;

    // a *= x^yf
    if (yf != 0) {
        if (yf > 0.5) {
            yf -= 1;
            yi += 1;
        }
        a1 = @exp(yf * @log(x));
    }

    // a *= x^yi
    const r2 = math.frexp(x);
    var xe = r2.exponent;
    var x1 = r2.significand;

    var i = @as(std.meta.Int(.signed, @typeInfo(T).Float.bits), @intFromFloat(yi));
    while (i != 0) : (i >>= 1) {
        const overflow_shift = math.floatExponentBits(T) + 1;
        if (xe < -(1 << overflow_shift) or (1 << overflow_shift) < xe) {
            // catch xe before it overflows the left shift below
            // Since i != 0 it has at least one bit still set, so ae will accumulate xe
            // on at least one more iteration, ae += xe is a lower bound on ae
            // the lower bound on ae exceeds the size of a float exp
            // so the final call to Ldexp will produce under/overflow (0/Inf)
            ae += xe;
            break;
        }
        if (i & 1 == 1) {
            a1 *= x1;
            ae += xe;
        }
        x1 *= x1;
        xe <<= 1;
        if (x1 < 0.5) {
            x1 += x1;
            xe -= 1;
        }
    }

    // a *= a1 * 2^ae
    if (y < 0) {
        a1 = 1 / a1;
        ae = -ae;
    }

    return math.scalbn(a1, ae);
}

fn isOddInteger(x: f64) bool {
    if (@abs(x) >= 1 << 53) {
        // From https://golang.org/src/math/pow.go
        // 1 << 53 is the largest exact integer in the float64 format.
        // Any number outside this range will be truncated before the decimal point and therefore will always be
        // an even integer.
        // Without this check and if x overflows i64 the @intFromFloat(r.ipart) conversion below will panic
        return false;
    }
    const r = math.modf(x);
    return r.fpart == 0.0 and @as(i64, @intFromFloat(r.ipart)) & 1 == 1;
}
