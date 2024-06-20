const std = @import("../../std.zig");
const math = std.math;
const cmath = math.complex;
const Complex = cmath.Complex;

/// Returns the angular component (in radians) of z.
pub fn arg(z: anytype) @TypeOf(z.re, z.im) {
    return math.atan2(z.im, z.re);
}

const epsilon = 0.0001;
