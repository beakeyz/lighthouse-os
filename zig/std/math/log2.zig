const std = @import("../std.zig");
const builtin = @import("../lightos/builtin.zig");
const math = std.math;

/// Returns the base-2 logarithm of x.
///
/// Special Cases:
///  - log2(+inf)  = +inf
///  - log2(0)     = -inf
///  - log2(x)     = nan if x < 0
///  - log2(nan)   = nan
pub fn log2(x: anytype) @TypeOf(x) {
    const T = @TypeOf(x);
    switch (@typeInfo(T)) {
        .ComptimeFloat => {
            return @as(comptime_float, @log2(x));
        },
        .Float => return @log2(x),
        .ComptimeInt => comptime {
            var x_shifted = x;
            // First, calculate floorPowerOfTwo(x)
            var shift_amt = 1;
            while (x_shifted >> (shift_amt << 1) != 0) shift_amt <<= 1;

            // Answer is in the range [shift_amt, 2 * shift_amt - 1]
            // We can find it in O(log(N)) using binary search.
            var result = 0;
            while (shift_amt != 0) : (shift_amt >>= 1) {
                if (x_shifted >> shift_amt != 0) {
                    x_shifted >>= shift_amt;
                    result += shift_amt;
                }
            }
            return result;
        },
        .Int => |IntType| switch (IntType.signedness) {
            .signed => @compileError("log2 not implemented for signed integers"),
            .unsigned => return math.log2_int(T, x),
        },
        else => @compileError("log2 not implemented for " ++ @typeName(T)),
    }
}
