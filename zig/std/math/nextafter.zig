const std = @import("../std.zig");
const math = std.math;
const assert = std.assert;

/// Returns the next representable value after `x` in the direction of `y`.
///
/// Special cases:
///
/// - If `x == y`, `y` is returned.
/// - For floats, if either `x` or `y` is a NaN, a NaN is returned.
/// - For floats, if `x == 0.0` and `@abs(y) > 0.0`, the smallest subnormal number with the sign of
///   `y` is returned.
///
pub fn nextAfter(comptime T: type, x: T, y: T) T {
    return switch (@typeInfo(T)) {
        .Int, .ComptimeInt => nextAfterInt(T, x, y),
        .Float => nextAfterFloat(T, x, y),
        else => @compileError("expected int or non-comptime float, found '" ++ @typeName(T) ++ "'"),
    };
}

fn nextAfterInt(comptime T: type, x: T, y: T) T {
    comptime assert(@typeInfo(T) == .Int or @typeInfo(T) == .ComptimeInt);
    return if (@typeInfo(T) == .Int and @bitSizeOf(T) < 2)
        // Special case for `i0`, `u0`, `i1`, and `u1`.
        y
    else if (y > x)
        x + 1
    else if (y < x)
        x - 1
    else
        y;
}

// Based on nextafterf/nextafterl from mingw-w64 which are both public domain.
// <https://github.com/mingw-w64/mingw-w64/blob/e89de847dd3e05bb8e46344378ce3e124f4e7d1c/mingw-w64-crt/math/nextafterf.c>
// <https://github.com/mingw-w64/mingw-w64/blob/e89de847dd3e05bb8e46344378ce3e124f4e7d1c/mingw-w64-crt/math/nextafterl.c>

fn nextAfterFloat(comptime T: type, x: T, y: T) T {
    comptime assert(@typeInfo(T) == .Float);
    if (x == y) {
        // Returning `y` ensures that (0.0, -0.0) returns -0.0 and that (-0.0, 0.0) returns 0.0.
        return y;
    }
    if (math.isNan(x) or math.isNan(y)) {
        return math.nan(T);
    }
    if (x == 0.0) {
        return if (y > 0.0)
            math.floatTrueMin(T)
        else
            -math.floatTrueMin(T);
    }
    if (@bitSizeOf(T) == 80) {
        // Unlike other floats, `f80` has an explicitly stored integer bit between the fractional
        // part and the exponent and thus requires special handling. This integer bit *must* be set
        // when the value is normal, an infinity or a NaN and *should* be cleared otherwise.

        const fractional_bits_mask = (1 << math.floatFractionalBits(f80)) - 1;
        const integer_bit_mask = 1 << math.floatFractionalBits(f80);
        const exponent_bits_mask = (1 << math.floatExponentBits(f80)) - 1;

        var x_parts = math.break_f80(x);

        // Bitwise increment/decrement the fractional part while also taking care to update the
        // exponent if we overflow the fractional part. This might flip the integer bit; this is
        // intentional.
        if ((x > 0.0) == (y > x)) {
            x_parts.fraction +%= 1;
            if (x_parts.fraction & fractional_bits_mask == 0) {
                x_parts.exp += 1;
            }
        } else {
            if (x_parts.fraction & fractional_bits_mask == 0) {
                x_parts.exp -= 1;
            }
            x_parts.fraction -%= 1;
        }

        // If the new value is normal or an infinity (indicated by at least one bit in the exponent
        // being set), the integer bit might have been cleared from an overflow, so we must ensure
        // that it remains set.
        if (x_parts.exp & exponent_bits_mask != 0) {
            x_parts.fraction |= integer_bit_mask;
        }
        // Otherwise, the new value is subnormal and the integer bit will have either flipped from
        // set to cleared (if the old value was normal) or remained cleared (if the old value was
        // subnormal), both of which are the outcomes we want.

        return math.make_f80(x_parts);
    } else {
        const Bits = std.meta.Int(.unsigned, @bitSizeOf(T));
        var x_bits: Bits = @bitCast(x);
        if ((x > 0.0) == (y > x)) {
            x_bits += 1;
        } else {
            x_bits -= 1;
        }
        return @bitCast(x_bits);
    }
}
