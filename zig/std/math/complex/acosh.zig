const std = @import("../../std.zig");
const math = std.math;
const cmath = math.complex;
const Complex = cmath.Complex;

/// Returns the hyperbolic arc-cosine of z.
pub fn acosh(z: anytype) Complex(@TypeOf(z.re, z.im)) {
    const T = @TypeOf(z.re, z.im);
    const q = cmath.acos(z);
    return Complex(T).init(-q.im, q.re);
}

const epsilon = 0.0001;
