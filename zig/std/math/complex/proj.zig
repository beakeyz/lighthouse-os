const std = @import("../../std.zig");
const math = std.math;
const cmath = math.complex;
const Complex = cmath.Complex;

/// Returns the projection of z onto the riemann sphere.
pub fn proj(z: anytype) Complex(@TypeOf(z.re, z.im)) {
    const T = @TypeOf(z.re, z.im);

    if (math.isInf(z.re) or math.isInf(z.im)) {
        return Complex(T).init(math.inf(T), math.copysign(@as(T, 0.0), z.re));
    }

    return Complex(T).init(z.re, z.im);
}

const epsilon = 0.0001;
