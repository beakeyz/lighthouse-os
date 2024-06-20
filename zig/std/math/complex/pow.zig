const std = @import("../../std.zig");
const math = std.math;
const cmath = math.complex;
const Complex = cmath.Complex;

/// Returns z raised to the complex power of c.
pub fn pow(z: anytype, s: anytype) Complex(@TypeOf(z.re, z.im, s.re, s.im)) {
    return cmath.exp(cmath.log(z).mul(s));
}

const epsilon = 0.0001;
