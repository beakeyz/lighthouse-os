// Ported from musl, which is licensed under the MIT license:
// https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
//
// https://git.musl-libc.org/cgit/musl/tree/src/math/atan2f.c
// https://git.musl-libc.org/cgit/musl/tree/src/math/atan2.c

const std = @import("../std.zig");
const math = std.math;

/// Returns the arc-tangent of y/x.
///
///      Special Cases:
/// |   y   |   x   | radians |
/// |-------|-------|---------|
/// |  fin  |  nan  |   nan   |
/// |  nan  |  fin  |   nan   |
/// |  +0   | >=+0  |   +0    |
/// |  -0   | >=+0  |   -0    |
/// |  +0   | <=-0  |   pi    |
/// |  -0   | <=-0  |  -pi    |
/// |  pos  |   0   |  +pi/2  |
/// |  neg  |   0   |  -pi/2  |
/// | +inf  | +inf  |  +pi/4  |
/// | -inf  | +inf  |  -pi/4  |
/// | +inf  | -inf  |  3pi/4  |
/// | -inf  | -inf  | -3pi/4  |
/// |  fin  | +inf  |    0    |
/// |  pos  | -inf  |  +pi    |
/// |  neg  | -inf  |  -pi    |
/// | +inf  |  fin  |  +pi/2  |
/// | -inf  |  fin  |  -pi/2  |
pub fn atan2(y: anytype, x: anytype) @TypeOf(x, y) {
    const T = @TypeOf(x, y);
    return switch (T) {
        f32 => atan2_32(y, x),
        f64 => atan2_64(y, x),
        else => @compileError("atan2 not implemented for " ++ @typeName(T)),
    };
}

fn atan2_32(y: f32, x: f32) f32 {
    const pi: f32 = 3.1415927410e+00;
    const pi_lo: f32 = -8.7422776573e-08;

    if (math.isNan(x) or math.isNan(y)) {
        return x + y;
    }

    var ix = @as(u32, @bitCast(x));
    var iy = @as(u32, @bitCast(y));

    // x = 1.0
    if (ix == 0x3F800000) {
        return math.atan(y);
    }

    // 2 * sign(x) + sign(y)
    const m = ((iy >> 31) & 1) | ((ix >> 30) & 2);
    ix &= 0x7FFFFFFF;
    iy &= 0x7FFFFFFF;

    if (iy == 0) {
        switch (m) {
            0, 1 => return y, // atan(+-0, +...)
            2 => return pi, // atan(+0, -...)
            3 => return -pi, // atan(-0, -...)
            else => unreachable,
        }
    }

    if (ix == 0) {
        if (m & 1 != 0) {
            return -pi / 2;
        } else {
            return pi / 2;
        }
    }

    if (ix == 0x7F800000) {
        if (iy == 0x7F800000) {
            switch (m) {
                0 => return pi / 4, // atan(+inf, +inf)
                1 => return -pi / 4, // atan(-inf, +inf)
                2 => return 3 * pi / 4, // atan(+inf, -inf)
                3 => return -3 * pi / 4, // atan(-inf, -inf)
                else => unreachable,
            }
        } else {
            switch (m) {
                0 => return 0.0, // atan(+..., +inf)
                1 => return -0.0, // atan(-..., +inf)
                2 => return pi, // atan(+..., -inf)
                3 => return -pi, // atan(-...f, -inf)
                else => unreachable,
            }
        }
    }

    // |y / x| > 0x1p26
    if (ix + (26 << 23) < iy or iy == 0x7F800000) {
        if (m & 1 != 0) {
            return -pi / 2;
        } else {
            return pi / 2;
        }
    }

    // z = atan(|y / x|) with correct underflow
    const z = z: {
        if ((m & 2) != 0 and iy + (26 << 23) < ix) {
            break :z 0.0;
        } else {
            break :z math.atan(@abs(y / x));
        }
    };

    switch (m) {
        0 => return z, // atan(+, +)
        1 => return -z, // atan(-, +)
        2 => return pi - (z - pi_lo), // atan(+, -)
        3 => return (z - pi_lo) - pi, // atan(-, -)
        else => unreachable,
    }
}

fn atan2_64(y: f64, x: f64) f64 {
    const pi: f64 = 3.1415926535897931160E+00;
    const pi_lo: f64 = 1.2246467991473531772E-16;

    if (math.isNan(x) or math.isNan(y)) {
        return x + y;
    }

    const ux: u64 = @bitCast(x);
    var ix: u32 = @intCast(ux >> 32);
    const lx: u32 = @intCast(ux & 0xFFFFFFFF);

    const uy: u64 = @bitCast(y);
    var iy: u32 = @intCast(uy >> 32);
    const ly: u32 = @intCast(uy & 0xFFFFFFFF);

    // x = 1.0
    if ((ix -% 0x3FF00000) | lx == 0) {
        return math.atan(y);
    }

    // 2 * sign(x) + sign(y)
    const m = ((iy >> 31) & 1) | ((ix >> 30) & 2);
    ix &= 0x7FFFFFFF;
    iy &= 0x7FFFFFFF;

    if (iy | ly == 0) {
        switch (m) {
            0, 1 => return y, // atan(+-0, +...)
            2 => return pi, // atan(+0, -...)
            3 => return -pi, // atan(-0, -...)
            else => unreachable,
        }
    }

    if (ix | lx == 0) {
        if (m & 1 != 0) {
            return -pi / 2;
        } else {
            return pi / 2;
        }
    }

    if (ix == 0x7FF00000) {
        if (iy == 0x7FF00000) {
            switch (m) {
                0 => return pi / 4, // atan(+inf, +inf)
                1 => return -pi / 4, // atan(-inf, +inf)
                2 => return 3 * pi / 4, // atan(+inf, -inf)
                3 => return -3 * pi / 4, // atan(-inf, -inf)
                else => unreachable,
            }
        } else {
            switch (m) {
                0 => return 0.0, // atan(+..., +inf)
                1 => return -0.0, // atan(-..., +inf)
                2 => return pi, // atan(+..., -inf)
                3 => return -pi, // atan(-...f, -inf)
                else => unreachable,
            }
        }
    }

    // |y / x| > 0x1p64
    if (ix +% (64 << 20) < iy or iy == 0x7FF00000) {
        if (m & 1 != 0) {
            return -pi / 2;
        } else {
            return pi / 2;
        }
    }

    // z = atan(|y / x|) with correct underflow
    const z = z: {
        if ((m & 2) != 0 and iy +% (64 << 20) < ix) {
            break :z 0.0;
        } else {
            break :z math.atan(@abs(y / x));
        }
    };

    switch (m) {
        0 => return z, // atan(+, +)
        1 => return -z, // atan(-, +)
        2 => return pi - (z - pi_lo), // atan(+, -)
        3 => return (z - pi_lo) - pi, // atan(-, -)
        else => unreachable,
    }
}
