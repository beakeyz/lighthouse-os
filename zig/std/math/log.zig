// Ported from musl, which is licensed under the MIT license:
// https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
//
// https://git.musl-libc.org/cgit/musl/tree/src/math/logf.c
// https://git.musl-libc.org/cgit/musl/tree/src/math/log.c

const std = @import("../std.zig");
const math = std.math;

/// Returns the logarithm of x for the provided base.
pub fn log(comptime T: type, base: T, x: T) T {
    if (base == 2) {
        return math.log2(x);
    } else if (base == 10) {
        return math.log10(x);
    } else if ((@typeInfo(T) == .Float or @typeInfo(T) == .ComptimeFloat) and base == math.e) {
        return @log(x);
    }

    const float_base = math.lossyCast(f64, base);
    switch (@typeInfo(T)) {
        .ComptimeFloat => {
            return @as(comptime_float, @log(@as(f64, x)) / @log(float_base));
        },

        .ComptimeInt => {
            return @as(comptime_int, math.log_int(comptime_int, base, x));
        },

        .Int => |IntType| switch (IntType.signedness) {
            .signed => @compileError("log not implemented for signed integers"),
            .unsigned => return @as(T, math.log_int(T, base, x)),
        },

        .Float => {
            switch (T) {
                f32 => return @as(f32, @floatCast(@log(@as(f64, x)) / @log(float_base))),
                f64 => return @log(x) / @log(float_base),
                else => @compileError("log not implemented for " ++ @typeName(T)),
            }
        },

        else => {
            @compileError("log expects integer or float, found '" ++ @typeName(T) ++ "'");
        },
    }
}
