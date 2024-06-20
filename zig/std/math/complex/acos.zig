const std = @import("../../std.zig");
const math = std.math;
const cmath = math.complex;
const Complex = cmath.Complex;

/// Returns the arc-cosine of z.
pub fn acos(z: anytype) Complex(@TypeOf(z.re, z.im)) {
    const T = @TypeOf(z.re, z.im);
    const q = cmath.asin(z);
    return Complex(T).init(@as(T, math.pi) / 2 - q.re, -q.im);
}

const epsilon = 0.0001;
