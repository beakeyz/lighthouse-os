const std = @import("../std.zig");
const math = std.math;

/// Returns a value with the magnitude of `magnitude` and the sign of `sign`.
pub fn copysign(magnitude: anytype, sign: @TypeOf(magnitude)) @TypeOf(magnitude) {
    const T = @TypeOf(magnitude);
    const TBits = std.meta.Int(.unsigned, @typeInfo(T).Float.bits);
    const sign_bit_mask = @as(TBits, 1) << (@bitSizeOf(T) - 1);
    const mag = @as(TBits, @bitCast(magnitude)) & ~sign_bit_mask;
    const sgn = @as(TBits, @bitCast(sign)) & sign_bit_mask;
    return @as(T, @bitCast(mag | sgn));
}
