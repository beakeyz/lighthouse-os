// Ported from musl, which is MIT licensed:
// https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
//
// https://git.musl-libc.org/cgit/musl/tree/src/math/frexpl.c
// https://git.musl-libc.org/cgit/musl/tree/src/math/frexpf.c
// https://git.musl-libc.org/cgit/musl/tree/src/math/frexp.c

const std = @import("../std.zig");
const math = std.math;

pub fn Frexp(comptime T: type) type {
    return struct {
        significand: T,
        exponent: i32,
    };
}

/// Breaks x into a normalized fraction and an integral power of two.
/// f == frac * 2^exp, with |frac| in the interval [0.5, 1).
///
/// Special Cases:
///  - frexp(+-0)   = +-0, 0
///  - frexp(+-inf) = +-inf, 0
///  - frexp(nan)   = nan, undefined
pub fn frexp(x: anytype) Frexp(@TypeOf(x)) {
    const T = @TypeOf(x);
    return switch (T) {
        f32 => frexp32(x),
        f64 => frexp64(x),
        f128 => frexp128(x),
        else => @compileError("frexp not implemented for " ++ @typeName(T)),
    };
}

// TODO: unify all these implementations using generics

fn frexp32(x: f32) Frexp(f32) {
    var result: Frexp(f32) = undefined;

    var y = @as(u32, @bitCast(x));
    const e = @as(i32, @intCast(y >> 23)) & 0xFF;

    if (e == 0) {
        if (x != 0) {
            // subnormal
            result = frexp32(x * 0x1.0p64);
            result.exponent -= 64;
        } else {
            // frexp(+-0) = (+-0, 0)
            result.significand = x;
            result.exponent = 0;
        }
        return result;
    } else if (e == 0xFF) {
        // frexp(nan) = (nan, undefined)
        result.significand = x;
        result.exponent = undefined;

        // frexp(+-inf) = (+-inf, 0)
        if (math.isInf(x)) {
            result.exponent = 0;
        }

        return result;
    }

    result.exponent = e - 0x7E;
    y &= 0x807FFFFF;
    y |= 0x3F000000;
    result.significand = @as(f32, @bitCast(y));
    return result;
}

fn frexp64(x: f64) Frexp(f64) {
    var result: Frexp(f64) = undefined;

    var y = @as(u64, @bitCast(x));
    const e = @as(i32, @intCast(y >> 52)) & 0x7FF;

    if (e == 0) {
        if (x != 0) {
            // subnormal
            result = frexp64(x * 0x1.0p64);
            result.exponent -= 64;
        } else {
            // frexp(+-0) = (+-0, 0)
            result.significand = x;
            result.exponent = 0;
        }
        return result;
    } else if (e == 0x7FF) {
        // frexp(nan) = (nan, undefined)
        result.significand = x;
        result.exponent = undefined;

        // frexp(+-inf) = (+-inf, 0)
        if (math.isInf(x)) {
            result.exponent = 0;
        }

        return result;
    }

    result.exponent = e - 0x3FE;
    y &= 0x800FFFFFFFFFFFFF;
    y |= 0x3FE0000000000000;
    result.significand = @as(f64, @bitCast(y));
    return result;
}

fn frexp128(x: f128) Frexp(f128) {
    var result: Frexp(f128) = undefined;

    var y = @as(u128, @bitCast(x));
    const e = @as(i32, @intCast(y >> 112)) & 0x7FFF;

    if (e == 0) {
        if (x != 0) {
            // subnormal
            result = frexp128(x * 0x1.0p120);
            result.exponent -= 120;
        } else {
            // frexp(+-0) = (+-0, 0)
            result.significand = x;
            result.exponent = 0;
        }
        return result;
    } else if (e == 0x7FFF) {
        // frexp(nan) = (nan, undefined)
        result.significand = x;
        result.exponent = undefined;

        // frexp(+-inf) = (+-inf, 0)
        if (math.isInf(x)) {
            result.exponent = 0;
        }

        return result;
    }

    result.exponent = e - 0x3FFE;
    y &= 0x8000FFFFFFFFFFFFFFFFFFFFFFFFFFFF;
    y |= 0x3FFE0000000000000000000000000000;
    result.significand = @as(f128, @bitCast(y));
    return result;
}
