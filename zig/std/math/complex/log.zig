const std = @import("../../std.zig");
const math = std.math;
const cmath = math.complex;
const Complex = cmath.Complex;

/// Returns the natural logarithm of z.
pub fn log(z: anytype) Complex(@TypeOf(z.re, z.im)) {
    const T = @TypeOf(z.re, z.im);
    const r = cmath.abs(z);
    const phi = cmath.arg(z);

    return Complex(T).init(@log(r), phi);
}

const epsilon = 0.0001;
