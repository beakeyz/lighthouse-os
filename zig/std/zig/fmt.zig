const std = @import("std");
const mem = std.mem;

/// Print the string as a Zig identifier escaping it with @"" syntax if needed.
fn formatId(
    bytes: []const u8,
    comptime fmt: []const u8,
    options: std.fmt.FormatOptions,
    writer: anytype,
) !void {
    _ = fmt;
    if (isValidId(bytes)) {
        return writer.writeAll(bytes);
    }
    try writer.writeAll("@\"");
    try stringEscape(bytes, "", options, writer);
    try writer.writeByte('"');
}

/// Return a Formatter for a Zig identifier
pub fn fmtId(bytes: []const u8) std.fmt.Formatter(formatId) {
    return .{ .data = bytes };
}

pub fn isValidId(bytes: []const u8) bool {
    if (bytes.len == 0) return false;
    if (mem.eql(u8, bytes, "_")) return false;
    for (bytes, 0..) |c, i| {
        switch (c) {
            '_', 'a'...'z', 'A'...'Z' => {},
            '0'...'9' => if (i == 0) return false,
            else => return false,
        }
    }
    return std.zig.Token.getKeyword(bytes) == null;
}

/// Print the string as escaped contents of a double quoted or single-quoted string.
/// Format `{}` treats contents as a double-quoted string.
/// Format `{'}` treats contents as a single-quoted string.
pub fn stringEscape(
    bytes: []const u8,
    comptime fmt: []const u8,
    options: std.fmt.FormatOptions,
    writer: anytype,
) !void {
    _ = options;
    for (bytes) |byte| switch (byte) {
        '\n' => try writer.writeAll("\\n"),
        '\r' => try writer.writeAll("\\r"),
        '\t' => try writer.writeAll("\\t"),
        '\\' => try writer.writeAll("\\\\"),
        '"' => {
            if (fmt.len == 1 and fmt[0] == '\'') {
                try writer.writeByte('"');
            } else if (fmt.len == 0) {
                try writer.writeAll("\\\"");
            } else {
                @compileError("expected {} or {'}, found {" ++ fmt ++ "}");
            }
        },
        '\'' => {
            if (fmt.len == 1 and fmt[0] == '\'') {
                try writer.writeAll("\\'");
            } else if (fmt.len == 0) {
                try writer.writeByte('\'');
            } else {
                @compileError("expected {} or {'}, found {" ++ fmt ++ "}");
            }
        },
        ' ', '!', '#'...'&', '('...'[', ']'...'~' => try writer.writeByte(byte),
        // Use hex escapes for rest any unprintable characters.
        else => {
            try writer.writeAll("\\x");
            try std.fmt.formatInt(byte, 16, .lower, .{ .width = 2, .fill = '0' }, writer);
        },
    };
}

/// Return a Formatter for Zig Escapes of a double quoted string.
/// The format specifier must be one of:
///  * `{}` treats contents as a double-quoted string.
///  * `{'}` treats contents as a single-quoted string.
pub fn fmtEscapes(bytes: []const u8) std.fmt.Formatter(stringEscape) {
    return .{ .data = bytes };
}
