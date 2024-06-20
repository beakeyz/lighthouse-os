pub const fmt = @import("zig/fmt.zig");

pub const ErrorBundle = @import("zig/ErrorBundle.zig");
pub const Server = @import("zig/Server.zig");
pub const Client = @import("zig/Client.zig");
pub const Token = tokenizer.Token;
pub const Tokenizer = tokenizer.Tokenizer;
pub const fmtId = fmt.fmtId;
pub const fmtEscapes = fmt.fmtEscapes;
pub const isValidId = fmt.isValidId;
pub const string_literal = @import("zig/string_literal.zig");
pub const number_literal = @import("zig/number_literal.zig");
pub const primitives = @import("zig/primitives.zig");
pub const Ast = @import("zig/Ast.zig");
pub const system = @import("zig/system.zig");
/// Deprecated: use `std.Target.Query`.
pub const CrossTarget = std.Target.Query;
pub const BuiltinFn = @import("zig/BuiltinFn.zig");
pub const AstRlAnnotate = @import("zig/AstRlAnnotate.zig");

// Files needed by translate-c.
pub const c_builtins = @import("zig/c_builtins.zig");
pub const c_translation = @import("zig/c_translation.zig");

pub const Loc = struct {
    line: usize,
    column: usize,
    /// Does not include the trailing newline.
    source_line: []const u8,

    pub fn eql(a: Loc, b: Loc) bool {
        return a.line == b.line and a.column == b.column and std.mem.eql(u8, a.source_line, b.source_line);
    }
};

pub fn findLineColumn(source: []const u8, byte_offset: usize) Loc {
    var line: usize = 0;
    var column: usize = 0;
    var line_start: usize = 0;
    var i: usize = 0;
    while (i < byte_offset) : (i += 1) {
        switch (source[i]) {
            '\n' => {
                line += 1;
                column = 0;
                line_start = i + 1;
            },
            else => {
                column += 1;
            },
        }
    }
    while (i < source.len and source[i] != '\n') {
        i += 1;
    }
    return .{
        .line = line,
        .column = column,
        .source_line = source[line_start..i],
    };
}

pub fn lineDelta(source: []const u8, start: usize, end: usize) isize {
    var line: isize = 0;
    if (end >= start) {
        for (source[start..end]) |byte| switch (byte) {
            '\n' => line += 1,
            else => continue,
        };
    } else {
        for (source[end..start]) |byte| switch (byte) {
            '\n' => line -= 1,
            else => continue,
        };
    }
    return line;
}

pub const BinNameOptions = struct {
    root_name: []const u8,
    target: std.Target,
    output_mode: std.builtin.OutputMode,
    link_mode: ?std.builtin.LinkMode = null,
    version: ?std.SemanticVersion = null,
};

/// Returns the standard file system basename of a binary generated by the Zig compiler.
pub fn binNameAlloc(allocator: Allocator, options: BinNameOptions) error{OutOfMemory}![]u8 {
    const root_name = options.root_name;
    const target = options.target;
    switch (target.ofmt) {
        .coff => switch (options.output_mode) {
            .Exe => return std.fmt.allocPrint(allocator, "{s}{s}", .{ root_name, target.exeFileExt() }),
            .Lib => {
                const suffix = switch (options.link_mode orelse .Static) {
                    .Static => ".lib",
                    .Dynamic => ".dll",
                };
                return std.fmt.allocPrint(allocator, "{s}{s}", .{ root_name, suffix });
            },
            .Obj => return std.fmt.allocPrint(allocator, "{s}.obj", .{root_name}),
        },
        .elf => switch (options.output_mode) {
            .Exe => return allocator.dupe(u8, root_name),
            .Lib => {
                switch (options.link_mode orelse .Static) {
                    .Static => return std.fmt.allocPrint(allocator, "{s}{s}.a", .{
                        target.libPrefix(), root_name,
                    }),
                    .Dynamic => {
                        if (options.version) |ver| {
                            return std.fmt.allocPrint(allocator, "{s}{s}.so.{d}.{d}.{d}", .{
                                target.libPrefix(), root_name, ver.major, ver.minor, ver.patch,
                            });
                        } else {
                            return std.fmt.allocPrint(allocator, "{s}{s}.so", .{
                                target.libPrefix(), root_name,
                            });
                        }
                    },
                }
            },
            .Obj => return std.fmt.allocPrint(allocator, "{s}.o", .{root_name}),
        },
        .macho => switch (options.output_mode) {
            .Exe => return allocator.dupe(u8, root_name),
            .Lib => {
                switch (options.link_mode orelse .Static) {
                    .Static => return std.fmt.allocPrint(allocator, "{s}{s}.a", .{
                        target.libPrefix(), root_name,
                    }),
                    .Dynamic => {
                        if (options.version) |ver| {
                            return std.fmt.allocPrint(allocator, "{s}{s}.{d}.{d}.{d}.dylib", .{
                                target.libPrefix(), root_name, ver.major, ver.minor, ver.patch,
                            });
                        } else {
                            return std.fmt.allocPrint(allocator, "{s}{s}.dylib", .{
                                target.libPrefix(), root_name,
                            });
                        }
                    },
                }
            },
            .Obj => return std.fmt.allocPrint(allocator, "{s}.o", .{root_name}),
        },
        .wasm => switch (options.output_mode) {
            .Exe => return std.fmt.allocPrint(allocator, "{s}{s}", .{ root_name, target.exeFileExt() }),
            .Lib => {
                switch (options.link_mode orelse .Static) {
                    .Static => return std.fmt.allocPrint(allocator, "{s}{s}.a", .{
                        target.libPrefix(), root_name,
                    }),
                    .Dynamic => return std.fmt.allocPrint(allocator, "{s}.wasm", .{root_name}),
                }
            },
            .Obj => return std.fmt.allocPrint(allocator, "{s}.o", .{root_name}),
        },
        .c => return std.fmt.allocPrint(allocator, "{s}.c", .{root_name}),
        .spirv => return std.fmt.allocPrint(allocator, "{s}.spv", .{root_name}),
        .hex => return std.fmt.allocPrint(allocator, "{s}.ihex", .{root_name}),
        .raw => return std.fmt.allocPrint(allocator, "{s}.bin", .{root_name}),
        .plan9 => switch (options.output_mode) {
            .Exe => return allocator.dupe(u8, root_name),
            .Obj => return std.fmt.allocPrint(allocator, "{s}{s}", .{
                root_name, target.ofmt.fileExt(target.cpu.arch),
            }),
            .Lib => return std.fmt.allocPrint(allocator, "{s}{s}.a", .{
                target.libPrefix(), root_name,
            }),
        },
        .nvptx => return std.fmt.allocPrint(allocator, "{s}.ptx", .{root_name}),
        .dxcontainer => return std.fmt.allocPrint(allocator, "{s}.dxil", .{root_name}),
    }
}

pub const BuildId = union(enum) {
    none,
    fast,
    uuid,
    sha1,
    md5,
    hexstring: HexString,

    pub fn eql(a: BuildId, b: BuildId) bool {
        const Tag = @typeInfo(BuildId).Union.tag_type.?;
        const a_tag: Tag = a;
        const b_tag: Tag = b;
        if (a_tag != b_tag) return false;
        return switch (a) {
            .none, .fast, .uuid, .sha1, .md5 => true,
            .hexstring => |a_hexstring| std.mem.eql(u8, a_hexstring.toSlice(), b.hexstring.toSlice()),
        };
    }

    pub const HexString = struct {
        bytes: [32]u8,
        len: u8,

        /// Result is byte values, *not* hex-encoded.
        pub fn toSlice(hs: *const HexString) []const u8 {
            return hs.bytes[0..hs.len];
        }
    };

    /// Input is byte values, *not* hex-encoded.
    /// Asserts `bytes` fits inside `HexString`
    pub fn initHexString(bytes: []const u8) BuildId {
        var result: BuildId = .{ .hexstring = .{
            .bytes = undefined,
            .len = @intCast(bytes.len),
        } };
        @memcpy(result.hexstring.bytes[0..bytes.len], bytes);
        return result;
    }

    /// Converts UTF-8 text to a `BuildId`.
    pub fn parse(text: []const u8) !BuildId {
        if (std.mem.eql(u8, text, "none")) {
            return .none;
        } else if (std.mem.eql(u8, text, "fast")) {
            return .fast;
        } else if (std.mem.eql(u8, text, "uuid")) {
            return .uuid;
        } else if (std.mem.eql(u8, text, "sha1") or std.mem.eql(u8, text, "tree")) {
            return .sha1;
        } else if (std.mem.eql(u8, text, "md5")) {
            return .md5;
        } else if (std.mem.startsWith(u8, text, "0x")) {
            var result: BuildId = .{ .hexstring = undefined };
            const slice = try std.fmt.hexToBytes(&result.hexstring.bytes, text[2..]);
            result.hexstring.len = @as(u8, @intCast(slice.len));
            return result;
        }
        return error.InvalidBuildIdStyle;
    }
};

/// Renders a `std.Target.Cpu` value into a textual representation that can be parsed
/// via the `-mcpu` flag passed to the Zig compiler.
/// Appends the result to `buffer`.
pub fn serializeCpu(buffer: *std.ArrayList(u8), cpu: std.Target.Cpu) Allocator.Error!void {
    const all_features = cpu.arch.allFeaturesList();
    var populated_cpu_features = cpu.model.features;
    populated_cpu_features.populateDependencies(all_features);

    try buffer.appendSlice(cpu.model.name);

    if (populated_cpu_features.eql(cpu.features)) {
        // The CPU name alone is sufficient.
        return;
    }

    for (all_features, 0..) |feature, i_usize| {
        const i: std.Target.Cpu.Feature.Set.Index = @intCast(i_usize);
        const in_cpu_set = populated_cpu_features.isEnabled(i);
        const in_actual_set = cpu.features.isEnabled(i);
        try buffer.ensureUnusedCapacity(feature.name.len + 1);
        if (in_cpu_set and !in_actual_set) {
            buffer.appendAssumeCapacity('-');
            buffer.appendSliceAssumeCapacity(feature.name);
        } else if (!in_cpu_set and in_actual_set) {
            buffer.appendAssumeCapacity('+');
            buffer.appendSliceAssumeCapacity(feature.name);
        }
    }
}

pub fn serializeCpuAlloc(ally: Allocator, cpu: std.Target.Cpu) Allocator.Error![]u8 {
    var buffer = std.ArrayList(u8).init(ally);
    try serializeCpu(&buffer, cpu);
    return buffer.toOwnedSlice();
}

const std = @import("std.zig");
const tokenizer = @import("zig/tokenizer.zig");
const assert = std.assert;
const Allocator = std.mem.Allocator;
