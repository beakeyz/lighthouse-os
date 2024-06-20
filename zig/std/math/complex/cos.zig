const std = @import("../../std.zig");
const math = std.math;
const cmath = math.complex;
const Complex = cmath.Complex;

/// Returns the cosine of z.
pub fn cos(z: anytype) Complex(@TypeOf(z.re, z.im)) {
    const T = @TypeOf(z.re, z.im);
    const p = Complex(T).init(-z.im, z.re);
    return cmath.cosh(p);
}

const epsilon = 0.0001;
