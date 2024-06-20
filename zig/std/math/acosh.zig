// Ported from musl, which is licensed under the MIT license:
// https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
//
// https://git.musl-libc.org/cgit/musl/tree/src/math/acoshf.c
// https://git.musl-libc.org/cgit/musl/tree/src/math/acosh.c

const std = @import("../std.zig");
const math = std.math;

/// Returns the hyperbolic arc-cosine of x.
///
/// Special cases:
///  - acosh(x)   = nan if x < 1
///  - acosh(nan) = nan
pub fn acosh(x: anytype) @TypeOf(x) {
    const T = @TypeOf(x);
    return switch (T) {
        f32 => acosh32(x),
        f64 => acosh64(x),
        else => @compileError("acosh not implemented for " ++ @typeName(T)),
    };
}

// acosh(x) = log(x + sqrt(x * x - 1))
fn acosh32(x: f32) f32 {
    const u = @as(u32, @bitCast(x));
    const i = u & 0x7FFFFFFF;

    // |x| < 2, invalid if x < 1 or nan
    if (i < 0x3F800000 + (1 << 23)) {
        return math.log1p(x - 1 + @sqrt((x - 1) * (x - 1) + 2 * (x - 1)));
    }
    // |x| < 0x1p12
    else if (i < 0x3F800000 + (12 << 23)) {
        return @log(2 * x - 1 / (x + @sqrt(x * x - 1)));
    }
    // |x| >= 0x1p12
    else {
        return @log(x) + 0.693147180559945309417232121458176568;
    }
}

fn acosh64(x: f64) f64 {
    const u = @as(u64, @bitCast(x));
    const e = (u >> 52) & 0x7FF;

    // |x| < 2, invalid if x < 1 or nan
    if (e < 0x3FF + 1) {
        return math.log1p(x - 1 + @sqrt((x - 1) * (x - 1) + 2 * (x - 1)));
    }
    // |x| < 0x1p26
    else if (e < 0x3FF + 26) {
        return @log(2 * x - 1 / (x + @sqrt(x * x - 1)));
    }
    // |x| >= 0x1p26 or nan
    else {
        return @log(x) + 0.693147180559945309417232121458176568;
    }
}
