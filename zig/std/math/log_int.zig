const std = @import("../std.zig");
const math = std.math;
const assert = std.assert;
const Log2Int = math.Log2Int;

/// Returns the logarithm of `x` for the provided `base`, rounding down to the nearest integer.
/// Asserts that `base > 1` and `x > 0`.
pub fn log_int(comptime T: type, base: T, x: T) Log2Int(T) {
    const valid = switch (@typeInfo(T)) {
        .ComptimeInt => true,
        .Int => |IntType| IntType.signedness == .unsigned,
        else => false,
    };
    if (!valid) @compileError("log_int requires an unsigned integer, found " ++ @typeName(T));

    assert(base > 1 and x > 0);
    if (base == 2) return math.log2_int(T, x);

    // Let's denote by [y] the integer part of y.

    // Throughout the iteration the following invariant is preserved:
    //     power = base ^ exponent

    // Safety and termination.
    //
    // We never overflow inside the loop because when we enter the loop we have
    //     power <= [maxInt(T) / base]
    // therefore
    //     power * base <= maxInt(T)
    // is a valid multiplication for type `T` and
    //     exponent + 1 <= log(base, maxInt(T)) <= log2(maxInt(T)) <= maxInt(Log2Int(T))
    // is a valid addition for type `Log2Int(T)`.
    //
    // This implies also termination because power is strictly increasing,
    // hence it must eventually surpass [x / base] < maxInt(T) and we then exit the loop.

    var exponent: Log2Int(T) = 0;
    var power: T = 1;
    while (power <= x / base) {
        power *= base;
        exponent += 1;
    }

    // If we never entered the loop we must have
    //     [x / base] < 1
    // hence
    //     x <= [x / base] * base < base
    // thus the result is 0. We can then return exponent, which is still 0.
    //
    // Otherwise, if we entered the loop at least once,
    // when we exit the loop we have that power is exactly divisible by base and
    //     power / base <= [x / base] < power
    // hence
    //     power <= [x / base] * base <= x < power * base
    // This means that
    //     base^exponent <= x < base^(exponent+1)
    // hence the result is exponent.

    return exponent;
}
