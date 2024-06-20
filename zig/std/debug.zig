const std = @import("std.zig");

pub fn print(comptime fmt: []const u8, args: anytype) void {
    _ = fmt;
    _ = args;
}

// `panicExtra` is useful when you want to print out an `@errorReturnTrace`
/// and also print out some values.
pub fn panicExtra(
    trace: ?*std.builtin.StackTrace,
    ret_addr: ?usize,
    comptime format: []const u8,
    args: anytype,
) noreturn {
    @setCold(true);

    const size = 0x1000;
    const trunc_msg = "(msg truncated)";
    var buf: [size + trunc_msg.len]u8 = undefined;
    // a minor annoyance with this is that it will result in the NoSpaceLeft
    // error being part of the @panic stack trace (but that error should
    // only happen rarely)
    const msg = std.fmt.bufPrint(buf[0..size], format, args) catch |err| switch (err) {
        error.NoSpaceLeft => blk: {
            @memcpy(buf[size..], trunc_msg);
            break :blk &buf;
        },
    };
    std.builtin.panic(msg, trace, ret_addr);
}

// `panicImpl` could be useful in implementing a custom panic handler which
// calls the default handler (on supported platforms)
pub fn panicImpl(trace: ?*const std.builtin.StackTrace, first_trace_addr: ?usize, msg: []const u8) noreturn {
    @setCold(true);

    std.builtin.panic(msg, trace, first_trace_addr);
}

pub fn assert(cond: bool) void {
    if (!cond)
        std.lightos_abi.kernel_panic("[ZIG_DRIVER] assert failed!");
}
