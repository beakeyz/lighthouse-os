// Based on Rust, which is licensed under the MIT license.
// https://github.com/rust-lang/rust/blob/360432f1e8794de58cd94f34c9c17ad65871e5b5/LICENSE-MIT
//
// https://github.com/rust-lang/rust/blob/360432f1e8794de58cd94f34c9c17ad65871e5b5/src/libcore/num/mod.rs#L3423

const std = @import("../std.zig");
const math = std.math;
const assert = std.assert;

/// Returns the power of x raised by the integer y (x^y).
///
/// Errors:
///  - Overflow: Integer overflow or Infinity
///  - Underflow: Absolute value of result smaller than 1
/// Edge case rules ordered by precedence:
///  - powi(T, x, 0)   = 1 unless T is i1, i0, u0
///  - powi(T, 0, x)   = 0 when x > 0
///  - powi(T, 0, x)   = Overflow
///  - powi(T, 1, y)   = 1
///  - powi(T, -1, y)  = -1 for y an odd integer
///  - powi(T, -1, y)  = 1 unless T is i1, i0, u0
///  - powi(T, -1, y)  = Overflow
///  - powi(T, x, y)   = Overflow when y >= @bitSizeOf(x)
///  - powi(T, x, y)   = Underflow when y < 0
pub fn powi(comptime T: type, x: T, y: T) (error{
    Overflow,
    Underflow,
}!T) {
    const bit_size = @typeInfo(T).Int.bits;

    // `y & 1 == 0` won't compile when `does_one_overflow`.
    const does_one_overflow = math.maxInt(T) < 1;
    const is_y_even = !does_one_overflow and y & 1 == 0;

    if (x == 1 or y == 0 or (x == -1 and is_y_even)) {
        if (does_one_overflow) {
            return error.Overflow;
        } else {
            return 1;
        }
    }

    if (x == -1) {
        return -1;
    }

    if (x == 0) {
        if (y > 0) {
            return 0;
        } else {
            // Infinity/NaN, not overflow in strict sense
            return error.Overflow;
        }
    }
    // x >= 2 or x <= -2 from this point
    if (y >= bit_size) {
        return error.Overflow;
    }
    if (y < 0) {
        return error.Underflow;
    }

    // invariant :
    // return value = powi(T, base, exp) * acc;

    var base = x;
    var exp = y;
    var acc: T = if (does_one_overflow) unreachable else 1;

    while (exp > 1) {
        if (exp & 1 == 1) {
            const ov = @mulWithOverflow(acc, base);
            if (ov[1] != 0) return error.Overflow;
            acc = ov[0];
        }

        exp >>= 1;

        const ov = @mulWithOverflow(base, base);
        if (ov[1] != 0) return error.Overflow;
        base = ov[0];
    }

    if (exp == 1) {
        const ov = @mulWithOverflow(acc, base);
        if (ov[1] != 0) return error.Overflow;
        acc = ov[0];
    }

    return acc;
}
