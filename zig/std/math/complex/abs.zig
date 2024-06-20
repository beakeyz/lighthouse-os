const std = @import("../../std.zig");
const math = std.math;
const cmath = math.complex;
const Complex = cmath.Complex;

/// Returns the absolute value (modulus) of z.
pub fn abs(z: anytype) @TypeOf(z.re, z.im) {
    return math.hypot(z.re, z.im);
}

const epsilon = 0.0001;
