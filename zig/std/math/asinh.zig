// Ported from musl, which is licensed under the MIT license:
// https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
//
// https://git.musl-libc.org/cgit/musl/tree/src/math/asinhf.c
// https://git.musl-libc.org/cgit/musl/tree/src/math/asinh.c

const std = @import("../std.zig");
const math = std.math;
const mem = std.mem;

const maxInt = std.math.maxInt;

/// Returns the hyperbolic arc-sin of x.
///
/// Special Cases:
///  - asinh(+-0)   = +-0
///  - asinh(+-inf) = +-inf
///  - asinh(nan)   = nan
pub fn asinh(x: anytype) @TypeOf(x) {
    const T = @TypeOf(x);
    return switch (T) {
        f32 => asinh32(x),
        f64 => asinh64(x),
        else => @compileError("asinh not implemented for " ++ @typeName(T)),
    };
}

// asinh(x) = sign(x) * log(|x| + sqrt(x * x + 1)) ~= x - x^3/6 + o(x^5)
fn asinh32(x: f32) f32 {
    const u = @as(u32, @bitCast(x));
    const i = u & 0x7FFFFFFF;
    const s = u >> 31;

    var rx = @as(f32, @bitCast(i)); // |x|

    // |x| >= 0x1p12 or inf or nan
    if (i >= 0x3F800000 + (12 << 23)) {
        rx = @log(rx) + 0.69314718055994530941723212145817656;
    }
    // |x| >= 2
    else if (i >= 0x3F800000 + (1 << 23)) {
        rx = @log(2 * rx + 1 / (@sqrt(rx * rx + 1) + rx));
    }
    // |x| >= 0x1p-12, up to 1.6ulp error
    else if (i >= 0x3F800000 - (12 << 23)) {
        rx = math.log1p(rx + rx * rx / (@sqrt(rx * rx + 1) + 1));
    }
    // |x| < 0x1p-12, inexact if x != 0
    else {
        mem.doNotOptimizeAway(rx + 0x1.0p120);
    }

    return if (s != 0) -rx else rx;
}

fn asinh64(x: f64) f64 {
    const u = @as(u64, @bitCast(x));
    const e = (u >> 52) & 0x7FF;
    const s = u >> 63;

    var rx = @as(f64, @bitCast(u & (maxInt(u64) >> 1))); // |x|

    // |x| >= 0x1p26 or inf or nan
    if (e >= 0x3FF + 26) {
        rx = @log(rx) + 0.693147180559945309417232121458176568;
    }
    // |x| >= 2
    else if (e >= 0x3FF + 1) {
        rx = @log(2 * rx + 1 / (@sqrt(rx * rx + 1) + rx));
    }
    // |x| >= 0x1p-12, up to 1.6ulp error
    else if (e >= 0x3FF - 26) {
        rx = math.log1p(rx + rx * rx / (@sqrt(rx * rx + 1) + 1));
    }
    // |x| < 0x1p-12, inexact if x != 0
    else {
        mem.doNotOptimizeAway(rx + 0x1.0p120);
    }

    return if (s != 0) -rx else rx;
}
