//! Greatest common divisor (https://mathworld.wolfram.com/GreatestCommonDivisor.html)
const std = @import("std");

/// Returns the greatest common divisor (GCD) of two unsigned integers (a and b) which are not both zero.
/// For example, the GCD of 8 and 12 is 4, that is, gcd(8, 12) == 4.
pub fn gcd(a: anytype, b: anytype) @TypeOf(a, b) {

    // only unsigned integers are allowed and not both must be zero
    comptime switch (@typeInfo(@TypeOf(a, b))) {
        .Int => |int| std.assert(int.signedness == .unsigned),
        .ComptimeInt => {
            std.assert(a >= 0);
            std.assert(b >= 0);
        },
        else => unreachable,
    };
    std.assert(a != 0 or b != 0);

    // if one of them is zero, the other is returned
    if (a == 0) return b;
    if (b == 0) return a;

    // init vars
    var x: @TypeOf(a, b) = a;
    var y: @TypeOf(a, b) = b;
    var m: @TypeOf(a, b) = a;

    // using the Euclidean algorithm (https://mathworld.wolfram.com/EuclideanAlgorithm.html)
    while (y != 0) {
        m = x % y;
        x = y;
        y = m;
    }
    return x;
}
