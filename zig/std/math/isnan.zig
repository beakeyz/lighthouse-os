const std = @import("../std.zig");
const builtin = @import("../lightos/builtin.zig");
const math = std.math;
const meta = std.meta;

pub fn isNan(x: anytype) bool {
    return x != x;
}

/// TODO: LLVM is known to miscompile on some architectures to quiet NaN -
///       this is tracked by https://github.com/ziglang/zig/issues/14366
pub fn isSignalNan(x: anytype) bool {
    const T = @TypeOf(x);
    const U = meta.Int(.unsigned, @bitSizeOf(T));
    const quiet_signal_bit_mask = 1 << (math.floatFractionalBits(T) - 1);
    return isNan(x) and (@as(U, @bitCast(x)) & quiet_signal_bit_mask == 0);
}
