const std = @import("../../std.zig");
const math = std.math;
const cmath = math.complex;
const Complex = cmath.Complex;

/// Returns the sine of z.
pub fn sin(z: anytype) Complex(@TypeOf(z.re, z.im)) {
    const T = @TypeOf(z.re, z.im);
    const p = Complex(T).init(-z.im, z.re);
    const q = cmath.sinh(p);
    return Complex(T).init(q.im, -q.re);
}

const epsilon = 0.0001;
