// Ported from musl, which is MIT licensed.
// https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
//
// https://git.musl-libc.org/cgit/musl/tree/src/math/ilogbl.c
// https://git.musl-libc.org/cgit/musl/tree/src/math/ilogbf.c
// https://git.musl-libc.org/cgit/musl/tree/src/math/ilogb.c

const std = @import("../std.zig");
const math = std.math;

const maxInt = std.math.maxInt;
const minInt = std.math.minInt;

/// Returns the binary exponent of x as an integer.
///
/// Special Cases:
///  - ilogb(+-inf) = maxInt(i32)
///  - ilogb(+-0)   = minInt(i32)
///  - ilogb(nan)   = minInt(i32)
pub fn ilogb(x: anytype) i32 {
    const T = @TypeOf(x);
    return ilogbX(T, x);
}

pub const fp_ilogbnan = minInt(i32);
pub const fp_ilogb0 = minInt(i32);

fn ilogbX(comptime T: type, x: T) i32 {
    const typeWidth = @typeInfo(T).Float.bits;
    const significandBits = math.floatMantissaBits(T);
    const exponentBits = math.floatExponentBits(T);

    const Z = std.meta.Int(.unsigned, typeWidth);

    const signBit = (@as(Z, 1) << (significandBits + exponentBits));
    const maxExponent = ((1 << exponentBits) - 1);
    const exponentBias = (maxExponent >> 1);

    const absMask = signBit - 1;

    const u = @as(Z, @bitCast(x)) & absMask;
    const e: i32 = @intCast(u >> significandBits);

    if (e == 0) {
        if (u == 0) {
            math.raiseInvalid();
            return fp_ilogb0;
        }

        // offset sign bit, exponent bits, and integer bit (if present) + bias
        const offset = 1 + exponentBits + @as(comptime_int, @intFromBool(T == f80)) - exponentBias;
        return offset - @as(i32, @intCast(@clz(u)));
    }

    if (e == maxExponent) {
        math.raiseInvalid();
        if (u > @as(Z, @bitCast(math.inf(T)))) {
            return fp_ilogbnan; // u is a NaN
        } else return maxInt(i32);
    }

    return e - exponentBias;
}
