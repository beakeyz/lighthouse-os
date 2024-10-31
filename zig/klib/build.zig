const std = @import("std");

pub fn build(b: *std.Build) void {
    // Let zig know we're in the wild west
    const target_query = std.Target.Query{
        .cpu_arch = std.Target.Cpu.Arch.x86_64,
        .os_tag = std.Target.Os.Tag.freestanding,
        .abi = std.Target.Abi.none,
    };
    // Don't do anything weird please
    const optimize = b.standardOptimizeOption(.{});
}
