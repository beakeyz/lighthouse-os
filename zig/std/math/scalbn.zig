const std = @import("std");

/// Returns a * FLT_RADIX ^ exp.
///
/// Zig only supports binary base IEEE-754 floats. Hence FLT_RADIX=2, and this is an alias for ldexp.
pub const scalbn = @import("ldexp.zig").ldexp;
