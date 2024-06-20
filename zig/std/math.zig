const std = @import("std");
const assert = std.assert;

/// Euler's number (e)
pub const e = 2.71828182845904523536028747135266249775724709369995;

/// Archimedes' constant (π)
pub const pi = 3.14159265358979323846264338327950288419716939937510;

/// Phi or Golden ratio constant (Φ) = (1 + sqrt(5))/2
pub const phi = 1.6180339887498948482045868343656381177203091798057628621;

/// Circle constant (τ)
pub const tau = 2 * pi;

/// log2(e)
pub const log2e = 1.442695040888963407359924681001892137;

/// log10(e)
pub const log10e = 0.434294481903251827651128918916605082;

/// ln(2)
pub const ln2 = 0.693147180559945309417232121458176568;

/// ln(10)
pub const ln10 = 2.302585092994045684017991454684364208;

/// 2/sqrt(π)
pub const two_sqrtpi = 1.128379167095512573896158903121545172;

/// sqrt(2)
pub const sqrt2 = 1.414213562373095048801688724209698079;

/// 1/sqrt(2)
pub const sqrt1_2 = 0.707106781186547524400844362104849039;

pub const floatExponentBits = @import("math/float.zig").floatExponentBits;
pub const floatMantissaBits = @import("math/float.zig").floatMantissaBits;
pub const floatFractionalBits = @import("math/float.zig").floatFractionalBits;
pub const floatExponentMin = @import("math/float.zig").floatExponentMin;
pub const floatExponentMax = @import("math/float.zig").floatExponentMax;
pub const floatTrueMin = @import("math/float.zig").floatTrueMin;
pub const floatMin = @import("math/float.zig").floatMin;
pub const floatMax = @import("math/float.zig").floatMax;
pub const floatEps = @import("math/float.zig").floatEps;
pub const inf = @import("math/float.zig").inf;
pub const nan = @import("math/float.zig").nan;
pub const snan = @import("math/float.zig").snan;

pub const isNan = @import("math/isnan.zig").isNan;
pub const isSignalNan = @import("math/isnan.zig").isSignalNan;
pub const frexp = @import("math/frexp.zig").frexp;
pub const Frexp = @import("math/frexp.zig").Frexp;
pub const modf = @import("math/modf.zig").modf;
pub const modf32_result = @import("math/modf.zig").modf32_result;
pub const modf64_result = @import("math/modf.zig").modf64_result;
pub const copysign = @import("math/copysign.zig").copysign;
pub const isFinite = @import("math/isfinite.zig").isFinite;
pub const isInf = @import("math/isinf.zig").isInf;
pub const isPositiveInf = @import("math/isinf.zig").isPositiveInf;
pub const isNegativeInf = @import("math/isinf.zig").isNegativeInf;
pub const isPositiveZero = @import("math/iszero.zig").isPositiveZero;
pub const isNegativeZero = @import("math/iszero.zig").isNegativeZero;
pub const isNormal = @import("math/isnormal.zig").isNormal;
pub const nextAfter = @import("math/nextafter.zig").nextAfter;
pub const signbit = @import("math/signbit.zig").signbit;
pub const scalbn = @import("math/scalbn.zig").scalbn;
pub const ldexp = @import("math/ldexp.zig").ldexp;
pub const pow = @import("math/pow.zig").pow;
pub const powi = @import("math/powi.zig").powi;
pub const sqrt = @import("math/sqrt.zig").sqrt;
pub const cbrt = @import("math/cbrt.zig").cbrt;
pub const acos = @import("math/acos.zig").acos;
pub const asin = @import("math/asin.zig").asin;
pub const atan = @import("math/atan.zig").atan;
pub const atan2 = @import("math/atan2.zig").atan2;
pub const hypot = @import("math/hypot.zig").hypot;
pub const expm1 = @import("math/expm1.zig").expm1;
pub const ilogb = @import("math/ilogb.zig").ilogb;
pub const log = @import("math/log.zig").log;
pub const log2 = @import("math/log2.zig").log2;
pub const log10 = @import("math/log10.zig").log10;
pub const log10_int = @import("math/log10.zig").log10_int;
pub const log_int = @import("math/log_int.zig").log_int;
pub const log1p = @import("math/log1p.zig").log1p;
pub const asinh = @import("math/asinh.zig").asinh;
pub const acosh = @import("math/acosh.zig").acosh;
pub const atanh = @import("math/atanh.zig").atanh;
pub const sinh = @import("math/sinh.zig").sinh;
pub const cosh = @import("math/cosh.zig").cosh;
pub const tanh = @import("math/tanh.zig").tanh;
pub const gcd = @import("math/gcd.zig").gcd;
pub const gamma = @import("math/gamma.zig").gamma;
pub const lgamma = @import("math/gamma.zig").lgamma;

/// Returns the maximum value of integer type T.
pub fn maxInt(comptime T: type) comptime_int {
    const info = @typeInfo(T);
    const bit_count = info.Int.bits;
    if (bit_count == 0) return 0;
    return (1 << (bit_count - @intFromBool(info.Int.signedness == .signed))) - 1;
}

/// Returns the minimum value of integer type T.
pub fn minInt(comptime T: type) comptime_int {
    const info = @typeInfo(T);
    const bit_count = info.Int.bits;
    if (info.Int.signedness == .unsigned) return 0;
    if (bit_count == 0) return 0;
    return -(1 << (bit_count - 1));
}

/// See also `Order`.
pub const CompareOperator = enum {
    /// Less than (`<`)
    lt,
    /// Less than or equal (`<=`)
    lte,
    /// Equal (`==`)
    eq,
    /// Greater than or equal (`>=`)
    gte,
    /// Greater than (`>`)
    gt,
    /// Not equal (`!=`)
    neq,

    /// Reverse the direction of the comparison.
    /// Use when swapping the left and right hand operands.
    pub fn reverse(op: CompareOperator) CompareOperator {
        return switch (op) {
            .lt => .gt,
            .lte => .gte,
            .gt => .lt,
            .gte => .lte,
            .eq => .eq,
            .neq => .neq,
        };
    }
};

/// This function does the same thing as comparison operators, however the
/// operator is a runtime-known enum value. Works on any operands that
/// support comparison operators.
pub fn compare(a: anytype, op: CompareOperator, b: anytype) bool {
    return switch (op) {
        .lt => a < b,
        .lte => a <= b,
        .eq => a == b,
        .neq => a != b,
        .gt => a > b,
        .gte => a >= b,
    };
}

/// See also `CompareOperator`.
pub const Order = enum {
    /// Greater than (`>`)
    gt,

    /// Less than (`<`)
    lt,

    /// Equal (`==`)
    eq,

    pub fn invert(self: Order) Order {
        return switch (self) {
            .lt => .gt,
            .eq => .eq,
            .gt => .lt,
        };
    }

    pub fn compare(self: Order, op: CompareOperator) bool {
        return switch (self) {
            .lt => switch (op) {
                .lt => true,
                .lte => true,
                .eq => false,
                .gte => false,
                .gt => false,
                .neq => true,
            },
            .eq => switch (op) {
                .lt => false,
                .lte => true,
                .eq => true,
                .gte => true,
                .gt => false,
                .neq => false,
            },
            .gt => switch (op) {
                .lt => false,
                .lte => false,
                .eq => false,
                .gte => true,
                .gt => true,
                .neq => true,
            },
        };
    }
};

/// Given two numbers, this function returns the order they are with respect to each other.
pub fn order(a: anytype, b: anytype) Order {
    if (a == b) {
        return .eq;
    } else if (a < b) {
        return .lt;
    } else if (a > b) {
        return .gt;
    } else {
        unreachable;
    }
}

/// Returns the product of a and b. Returns an error on overflow.
pub fn mul(comptime T: type, a: T, b: T) (error{Overflow}!T) {
    if (T == comptime_int) return a * b;
    const ov = @mulWithOverflow(a, b);
    if (ov[1] != 0) return error.Overflow;
    return ov[0];
}

/// Returns the sum of a and b. Returns an error on overflow.
pub fn add(comptime T: type, a: T, b: T) (error{Overflow}!T) {
    if (T == comptime_int) return a + b;
    const ov = @addWithOverflow(a, b);
    if (ov[1] != 0) return error.Overflow;
    return ov[0];
}

/// Returns a - b, or an error on overflow.
pub fn sub(comptime T: type, a: T, b: T) (error{Overflow}!T) {
    if (T == comptime_int) return a - b;
    const ov = @subWithOverflow(a, b);
    if (ov[1] != 0) return error.Overflow;
    return ov[0];
}

pub fn negate(x: anytype) !@TypeOf(x) {
    return sub(@TypeOf(x), 0, x);
}

/// Shifts a left by shift_amt. Returns an error on overflow. shift_amt
/// is unsigned.
pub fn shlExact(comptime T: type, a: T, shift_amt: Log2Int(T)) !T {
    if (T == comptime_int) return a << shift_amt;
    const ov = @shlWithOverflow(a, shift_amt);
    if (ov[1] != 0) return error.Overflow;
    return ov[0];
}

/// Shifts left. Overflowed bits are truncated.
/// A negative shift amount results in a right shift.
pub fn shl(comptime T: type, a: T, shift_amt: anytype) T {
    const abs_shift_amt = @abs(shift_amt);

    const casted_shift_amt = blk: {
        if (@typeInfo(T) == .Vector) {
            const C = @typeInfo(T).Vector.child;
            const len = @typeInfo(T).Vector.len;
            if (abs_shift_amt >= @typeInfo(C).Int.bits) return @splat(0);
            break :blk @as(@Vector(len, Log2Int(C)), @splat(@as(Log2Int(C), @intCast(abs_shift_amt))));
        } else {
            if (abs_shift_amt >= @typeInfo(T).Int.bits) return 0;
            break :blk @as(Log2Int(T), @intCast(abs_shift_amt));
        }
    };

    if (@TypeOf(shift_amt) == comptime_int or @typeInfo(@TypeOf(shift_amt)).Int.signedness == .signed) {
        if (shift_amt < 0) {
            return a >> casted_shift_amt;
        }
    }

    return a << casted_shift_amt;
}

/// Returns an unsigned int type that can hold the number of bits in T
/// - 1. Suitable for 0-based bit indices of T.
pub fn Log2Int(comptime T: type) type {
    // comptime ceil log2
    if (T == comptime_int) return comptime_int;
    comptime var count = 0;
    comptime var s = @typeInfo(T).Int.bits - 1;
    inline while (s != 0) : (s >>= 1) {
        count += 1;
    }

    return std.meta.Int(.unsigned, count);
}

/// Returns an unsigned int type that can hold the number of bits in T.
pub fn Log2IntCeil(comptime T: type) type {
    // comptime ceil log2
    if (T == comptime_int) return comptime_int;
    comptime var count = 0;
    comptime var s = @typeInfo(T).Int.bits;
    inline while (s != 0) : (s >>= 1) {
        count += 1;
    }

    return std.meta.Int(.unsigned, count);
}

/// Cast an integer to a different integer type. If the value doesn't fit,
/// return null.
pub fn cast(comptime T: type, x: anytype) ?T {
    comptime assert(@typeInfo(T) == .Int); // must pass an integer
    const is_comptime = @TypeOf(x) == comptime_int;
    comptime assert(is_comptime or @typeInfo(@TypeOf(x)) == .Int); // must pass an integer
    if ((is_comptime or maxInt(@TypeOf(x)) > maxInt(T)) and x > maxInt(T)) {
        return null;
    } else if ((is_comptime or minInt(@TypeOf(x)) < minInt(T)) and x < minInt(T)) {
        return null;
    } else {
        return @as(T, @intCast(x));
    }
}

/// Base-e exponential function on a floating point number.
/// Uses a dedicated hardware instruction when available.
/// This is the same as calling the builtin @exp
pub inline fn exp(value: anytype) @TypeOf(value) {
    return @exp(value);
}

/// Base-2 exponential function on a floating point number.
/// Uses a dedicated hardware instruction when available.
/// This is the same as calling the builtin @exp2
pub inline fn exp2(value: anytype) @TypeOf(value) {
    return @exp2(value);
}

pub const complex = @import("math/complex.zig");
pub const Complex = complex.Complex;

pub const big = @import("math/big.zig");

/// Cast a value to a different type. If the value doesn't fit in, or
/// can't be perfectly represented by, the new type, it will be
/// converted to the closest possible representation.
pub fn lossyCast(comptime T: type, value: anytype) T {
    switch (@typeInfo(T)) {
        .Float => {
            switch (@typeInfo(@TypeOf(value))) {
                .Int => return @as(T, @floatFromInt(value)),
                .Float => return @as(T, @floatCast(value)),
                .ComptimeInt => return @as(T, value),
                .ComptimeFloat => return @as(T, value),
                else => @compileError("bad type"),
            }
        },
        .Int => {
            switch (@typeInfo(@TypeOf(value))) {
                .Int, .ComptimeInt => {
                    if (value >= maxInt(T)) {
                        return @as(T, maxInt(T));
                    } else if (value <= minInt(T)) {
                        return @as(T, minInt(T));
                    } else {
                        return @as(T, @intCast(value));
                    }
                },
                .Float, .ComptimeFloat => {
                    if (isNan(value)) {
                        return 0;
                    } else if (value >= maxInt(T)) {
                        return @as(T, maxInt(T));
                    } else if (value <= minInt(T)) {
                        return @as(T, minInt(T));
                    } else {
                        return @as(T, @intFromFloat(value));
                    }
                },
                else => @compileError("bad type"),
            }
        },
        else => @compileError("bad result type"),
    }
}

/// Return the log base 2 of integer value x, rounding down to the
/// nearest integer.
pub fn log2_int(comptime T: type, x: T) Log2Int(T) {
    if (@typeInfo(T) != .Int or @typeInfo(T).Int.signedness != .unsigned)
        @compileError("log2_int requires an unsigned integer, found " ++ @typeName(T));
    assert(x != 0);
    return @as(Log2Int(T), @intCast(@typeInfo(T).Int.bits - 1 - @clz(x)));
}

/// Return the log base 2 of integer value x, rounding up to the
/// nearest integer.
pub fn log2_int_ceil(comptime T: type, x: T) Log2IntCeil(T) {
    if (@typeInfo(T) != .Int or @typeInfo(T).Int.signedness != .unsigned)
        @compileError("log2_int_ceil requires an unsigned integer, found " ++ @typeName(T));
    assert(x != 0);
    if (x == 1) return 0;
    const log2_val: Log2IntCeil(T) = log2_int(T, x - 1);
    return log2_val + 1;
}
