// Ported from musl, which is licensed under the MIT license:
// https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
//
// https://git.musl-libc.org/cgit/musl/tree/src/math/modff.c
// https://git.musl-libc.org/cgit/musl/tree/src/math/modf.c

const std = @import("../std.zig");
const math = std.math;

const maxInt = std.math.maxInt;

fn modf_result(comptime T: type) type {
    return struct {
        fpart: T,
        ipart: T,
    };
}
pub const modf32_result = modf_result(f32);
pub const modf64_result = modf_result(f64);

/// Returns the integer and fractional floating-point numbers that sum to x. The sign of each
/// result is the same as the sign of x.
///
/// Special Cases:
///  - modf(+-inf) = +-inf, nan
///  - modf(nan)   = nan, nan
pub fn modf(x: anytype) modf_result(@TypeOf(x)) {
    const T = @TypeOf(x);
    return switch (T) {
        f32 => modf32(x),
        f64 => modf64(x),
        else => @compileError("modf not implemented for " ++ @typeName(T)),
    };
}

fn modf32(x: f32) modf32_result {
    var result: modf32_result = undefined;

    const u: u32 = @bitCast(x);
    const e = @as(i32, @intCast((u >> 23) & 0xFF)) - 0x7F;
    const us = u & 0x80000000;

    // TODO: Shouldn't need this.
    if (math.isInf(x)) {
        result.ipart = x;
        result.fpart = math.nan(f32);
        return result;
    }

    // no fractional part
    if (e >= 23) {
        result.ipart = x;
        if (e == 0x80 and u << 9 != 0) { // nan
            result.fpart = x;
        } else {
            result.fpart = @as(f32, @bitCast(us));
        }
        return result;
    }

    // no integral part
    if (e < 0) {
        result.ipart = @as(f32, @bitCast(us));
        result.fpart = x;
        return result;
    }

    const mask = @as(u32, 0x007FFFFF) >> @as(u5, @intCast(e));
    if (u & mask == 0) {
        result.ipart = x;
        result.fpart = @as(f32, @bitCast(us));
        return result;
    }

    const uf: f32 = @bitCast(u & ~mask);
    result.ipart = uf;
    result.fpart = x - uf;
    return result;
}

fn modf64(x: f64) modf64_result {
    var result: modf64_result = undefined;

    const u: u64 = @bitCast(x);
    const e = @as(i32, @intCast((u >> 52) & 0x7FF)) - 0x3FF;
    const us = u & (1 << 63);

    if (math.isInf(x)) {
        result.ipart = x;
        result.fpart = math.nan(f64);
        return result;
    }

    // no fractional part
    if (e >= 52) {
        result.ipart = x;
        if (e == 0x400 and u << 12 != 0) { // nan
            result.fpart = x;
        } else {
            result.fpart = @as(f64, @bitCast(us));
        }
        return result;
    }

    // no integral part
    if (e < 0) {
        result.ipart = @as(f64, @bitCast(us));
        result.fpart = x;
        return result;
    }

    const mask = @as(u64, maxInt(u64) >> 12) >> @as(u6, @intCast(e));
    if (u & mask == 0) {
        result.ipart = x;
        result.fpart = @as(f64, @bitCast(us));
        return result;
    }

    const uf = @as(f64, @bitCast(u & ~mask));
    result.ipart = uf;
    result.fpart = x - uf;
    return result;
}
