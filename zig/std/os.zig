const std = @import("std.zig");

pub fn abort() void {
    std.lightos_abi.kernel_panic("std/os.zig: Nope this shit suckslmao");
}
