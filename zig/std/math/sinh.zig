// Ported from musl, which is licensed under the MIT license:
// https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
//
// https://git.musl-libc.org/cgit/musl/tree/src/math/sinhf.c
// https://git.musl-libc.org/cgit/musl/tree/src/math/sinh.c

const std = @import("../std.zig");
const math = std.math;

const expo2 = @import("expo2.zig").expo2;
const maxInt = std.math.maxInt;

/// Returns the hyperbolic sine of x.
///
/// Special Cases:
///  - sinh(+-0)   = +-0
///  - sinh(+-inf) = +-inf
///  - sinh(nan)   = nan
pub fn sinh(x: anytype) @TypeOf(x) {
    const T = @TypeOf(x);
    return switch (T) {
        f32 => sinh32(x),
        f64 => sinh64(x),
        else => @compileError("sinh not implemented for " ++ @typeName(T)),
    };
}

// sinh(x) = (exp(x) - 1 / exp(x)) / 2
//         = (exp(x) - 1 + (exp(x) - 1) / exp(x)) / 2
//         = x + x^3 / 6 + o(x^5)
fn sinh32(x: f32) f32 {
    const u = @as(u32, @bitCast(x));
    const ux = u & 0x7FFFFFFF;
    const ax = @as(f32, @bitCast(ux));

    if (x == 0.0 or math.isNan(x)) {
        return x;
    }

    var h: f32 = 0.5;
    if (u >> 31 != 0) {
        h = -h;
    }

    // |x| < log(FLT_MAX)
    if (ux < 0x42B17217) {
        const t = math.expm1(ax);
        if (ux < 0x3F800000) {
            if (ux < 0x3F800000 - (12 << 23)) {
                return x;
            } else {
                return h * (2 * t - t * t / (t + 1));
            }
        }
        return h * (t + t / (t + 1));
    }

    // |x| > log(FLT_MAX) or nan
    return 2 * h * expo2(ax);
}

fn sinh64(x: f64) f64 {
    const u = @as(u64, @bitCast(x));
    const w = @as(u32, @intCast(u >> 32)) & (maxInt(u32) >> 1);
    const ax = @as(f64, @bitCast(u & (maxInt(u64) >> 1)));

    if (x == 0.0 or math.isNan(x)) {
        return x;
    }

    var h: f32 = 0.5;
    if (u >> 63 != 0) {
        h = -h;
    }

    // |x| < log(FLT_MAX)
    if (w < 0x40862E42) {
        const t = math.expm1(ax);
        if (w < 0x3FF00000) {
            if (w < 0x3FF00000 - (26 << 20)) {
                return x;
            } else {
                return h * (2 * t - t * t / (t + 1));
            }
        }
        // NOTE: |x| > log(0x1p26) + eps could be h * exp(x)
        return h * (t + t / (t + 1));
    }

    // |x| > log(DBL_MAX) or nan
    return 2 * h * expo2(ax);
}
