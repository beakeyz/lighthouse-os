const std = @import("std");
const builtin = @import("../lightos/builtin.zig");
const math = std.math;
const mem = std.mem;

/// Given a type and value, cast the value to the type as c would.
pub fn cast(comptime DestType: type, target: anytype) DestType {
    // this function should behave like transCCast in translate-c, except it's for macros
    const SourceType = @TypeOf(target);
    switch (@typeInfo(DestType)) {
        .Fn => return castToPtr(*const DestType, SourceType, target),
        .Pointer => return castToPtr(DestType, SourceType, target),
        .Optional => |dest_opt| {
            if (@typeInfo(dest_opt.child) == .Pointer) {
                return castToPtr(DestType, SourceType, target);
            } else if (@typeInfo(dest_opt.child) == .Fn) {
                return castToPtr(?*const dest_opt.child, SourceType, target);
            }
        },
        .Int => {
            switch (@typeInfo(SourceType)) {
                .Pointer => {
                    return castInt(DestType, @intFromPtr(target));
                },
                .Optional => |opt| {
                    if (@typeInfo(opt.child) == .Pointer) {
                        return castInt(DestType, @intFromPtr(target));
                    }
                },
                .Int => {
                    return castInt(DestType, target);
                },
                .Fn => {
                    return castInt(DestType, @intFromPtr(&target));
                },
                .Bool => {
                    return @intFromBool(target);
                },
                else => {},
            }
        },
        .Float => {
            switch (@typeInfo(SourceType)) {
                .Int => return @as(DestType, @floatFromInt(target)),
                .Float => return @as(DestType, @floatCast(target)),
                .Bool => return @as(DestType, @floatFromInt(@intFromBool(target))),
                else => {},
            }
        },
        .Union => |info| {
            inline for (info.fields) |field| {
                if (field.type == SourceType) return @unionInit(DestType, field.name, target);
            }
            @compileError("cast to union type '" ++ @typeName(DestType) ++ "' from type '" ++ @typeName(SourceType) ++ "' which is not present in union");
        },
        .Bool => return cast(usize, target) != 0,
        else => {},
    }
    return @as(DestType, target);
}

fn castInt(comptime DestType: type, target: anytype) DestType {
    const dest = @typeInfo(DestType).Int;
    const source = @typeInfo(@TypeOf(target)).Int;

    if (dest.bits < source.bits)
        return @as(DestType, @bitCast(@as(std.meta.Int(source.signedness, dest.bits), @truncate(target))))
    else
        return @as(DestType, @bitCast(@as(std.meta.Int(source.signedness, dest.bits), target)));
}

fn castPtr(comptime DestType: type, target: anytype) DestType {
    return @constCast(@volatileCast(@alignCast(@ptrCast(target))));
}

fn castToPtr(comptime DestType: type, comptime SourceType: type, target: anytype) DestType {
    switch (@typeInfo(SourceType)) {
        .Int => {
            return @as(DestType, @ptrFromInt(castInt(usize, target)));
        },
        .ComptimeInt => {
            if (target < 0)
                return @as(DestType, @ptrFromInt(@as(usize, @bitCast(@as(isize, @intCast(target))))))
            else
                return @as(DestType, @ptrFromInt(@as(usize, @intCast(target))));
        },
        .Pointer => {
            return castPtr(DestType, target);
        },
        .Optional => |target_opt| {
            if (@typeInfo(target_opt.child) == .Pointer) {
                return castPtr(DestType, target);
            }
        },
        else => {},
    }
    return @as(DestType, target);
}

fn ptrInfo(comptime PtrType: type) std.builtin.Type.Pointer {
    return switch (@typeInfo(PtrType)) {
        .Optional => |opt_info| @typeInfo(opt_info.child).Pointer,
        .Pointer => |ptr_info| ptr_info,
        else => unreachable,
    };
}

/// Given a value returns its size as C's sizeof operator would.
pub fn sizeof(target: anytype) usize {
    const T: type = if (@TypeOf(target) == type) target else @TypeOf(target);
    switch (@typeInfo(T)) {
        .Float, .Int, .Struct, .Union, .Array, .Bool, .Vector => return @sizeOf(T),
        .Fn => {
            // sizeof(main) in C returns 1
            return 1;
        },
        .Null => return @sizeOf(*anyopaque),
        .Void => {
            // Note: sizeof(void) is 1 on clang/gcc and 0 on MSVC.
            return 1;
        },
        .Opaque => {
            if (T == anyopaque) {
                // Note: sizeof(void) is 1 on clang/gcc and 0 on MSVC.
                return 1;
            } else {
                @compileError("Cannot use C sizeof on opaque type " ++ @typeName(T));
            }
        },
        .Optional => |opt| {
            if (@typeInfo(opt.child) == .Pointer) {
                return sizeof(opt.child);
            } else {
                @compileError("Cannot use C sizeof on non-pointer optional " ++ @typeName(T));
            }
        },
        .Pointer => |ptr| {
            if (ptr.size == .Slice) {
                @compileError("Cannot use C sizeof on slice type " ++ @typeName(T));
            }
            // for strings, sizeof("a") returns 2.
            // normal pointer decay scenarios from C are handled
            // in the .Array case above, but strings remain literals
            // and are therefore always pointers, so they need to be
            // specially handled here.
            if (ptr.size == .One and ptr.is_const and @typeInfo(ptr.child) == .Array) {
                const array_info = @typeInfo(ptr.child).Array;
                if ((array_info.child == u8 or array_info.child == u16) and
                    array_info.sentinel != null and
                    @as(*align(1) const array_info.child, @ptrCast(array_info.sentinel.?)).* == 0)
                {
                    // length of the string plus one for the null terminator.
                    return (array_info.len + 1) * @sizeOf(array_info.child);
                }
            }
            // When zero sized pointers are removed, this case will no
            // longer be reachable and can be deleted.
            if (@sizeOf(T) == 0) {
                return @sizeOf(*anyopaque);
            }
            return @sizeOf(T);
        },
        .ComptimeFloat => return @sizeOf(f64), // TODO c_double #3999
        .ComptimeInt => {
            // TODO to get the correct result we have to translate
            // `1073741824 * 4` as `int(1073741824) *% int(4)` since
            // sizeof(1073741824 * 4) != sizeof(4294967296).

            // TODO test if target fits in int, long or long long
            return @sizeOf(c_int);
        },
        else => @compileError("std.meta.sizeof does not support type " ++ @typeName(T)),
    }
}

pub const CIntLiteralBase = enum { decimal, octal, hex };

/// Deprecated: use `CIntLiteralBase`
pub const CIntLiteralRadix = CIntLiteralBase;

fn PromoteIntLiteralReturnType(comptime SuffixType: type, comptime number: comptime_int, comptime base: CIntLiteralBase) type {
    const signed_decimal = [_]type{ c_int, c_long, c_longlong, c_ulonglong };
    const signed_oct_hex = [_]type{ c_int, c_uint, c_long, c_ulong, c_longlong, c_ulonglong };
    const unsigned = [_]type{ c_uint, c_ulong, c_ulonglong };

    const list: []const type = if (@typeInfo(SuffixType).Int.signedness == .unsigned)
        &unsigned
    else if (base == .decimal)
        &signed_decimal
    else
        &signed_oct_hex;

    var pos = mem.indexOfScalar(type, list, SuffixType).?;

    while (pos < list.len) : (pos += 1) {
        if (number >= math.minInt(list[pos]) and number <= math.maxInt(list[pos])) {
            return list[pos];
        }
    }
    @compileError("Integer literal is too large");
}

/// Promote the type of an integer literal until it fits as C would.
pub fn promoteIntLiteral(
    comptime SuffixType: type,
    comptime number: comptime_int,
    comptime base: CIntLiteralBase,
) PromoteIntLiteralReturnType(SuffixType, number, base) {
    return number;
}

/// Convert from clang __builtin_shufflevector index to Zig @shuffle index
/// clang requires __builtin_shufflevector index arguments to be integer constants.
/// negative values for `this_index` indicate "don't care" so we arbitrarily choose 0
/// clang enforces that `this_index` is less than the total number of vector elements
/// See https://ziglang.org/documentation/master/#shuffle
/// See https://clang.llvm.org/docs/LanguageExtensions.html#langext-builtin-shufflevector
pub fn shuffleVectorIndex(comptime this_index: c_int, comptime source_vector_len: usize) i32 {
    if (this_index <= 0) return 0;

    const positive_index = @as(usize, @intCast(this_index));
    if (positive_index < source_vector_len) return @as(i32, @intCast(this_index));
    const b_index = positive_index - source_vector_len;
    return ~@as(i32, @intCast(b_index));
}

/// Constructs a [*c] pointer with the const and volatile annotations
/// from SelfType for pointing to a C flexible array of ElementType.
pub fn FlexibleArrayType(comptime SelfType: type, comptime ElementType: type) type {
    switch (@typeInfo(SelfType)) {
        .Pointer => |ptr| {
            return @Type(.{ .Pointer = .{
                .size = .C,
                .is_const = ptr.is_const,
                .is_volatile = ptr.is_volatile,
                .alignment = @alignOf(ElementType),
                .address_space = .generic,
                .child = ElementType,
                .is_allowzero = true,
                .sentinel = null,
            } });
        },
        else => |info| @compileError("Invalid self type \"" ++ @tagName(info) ++ "\" for flexible array getter: " ++ @typeName(SelfType)),
    }
}

/// C `%` operator for signed integers
/// C standard states: "If the quotient a/b is representable, the expression (a/b)*b + a%b shall equal a"
/// The quotient is not representable if denominator is zero, or if numerator is the minimum integer for
/// the type and denominator is -1. C has undefined behavior for those two cases; this function has safety
/// checked undefined behavior
pub fn signedRemainder(numerator: anytype, denominator: anytype) @TypeOf(numerator, denominator) {
    std.assert(@typeInfo(@TypeOf(numerator, denominator)).Int.signedness == .signed);
    if (denominator > 0) return @rem(numerator, denominator);
    return numerator - @divTrunc(numerator, denominator) * denominator;
}

pub const Macros = struct {
    pub fn U_SUFFIX(comptime n: comptime_int) @TypeOf(promoteIntLiteral(c_uint, n, .decimal)) {
        return promoteIntLiteral(c_uint, n, .decimal);
    }

    fn L_SUFFIX_ReturnType(comptime number: anytype) type {
        switch (@typeInfo(@TypeOf(number))) {
            .Int, .ComptimeInt => return @TypeOf(promoteIntLiteral(c_long, number, .decimal)),
            .Float, .ComptimeFloat => return c_longdouble,
            else => @compileError("Invalid value for L suffix"),
        }
    }
    pub fn L_SUFFIX(comptime number: anytype) L_SUFFIX_ReturnType(number) {
        switch (@typeInfo(@TypeOf(number))) {
            .Int, .ComptimeInt => return promoteIntLiteral(c_long, number, .decimal),
            .Float, .ComptimeFloat => @compileError("TODO: c_longdouble initialization from comptime_float not supported"),
            else => @compileError("Invalid value for L suffix"),
        }
    }

    pub fn UL_SUFFIX(comptime n: comptime_int) @TypeOf(promoteIntLiteral(c_ulong, n, .decimal)) {
        return promoteIntLiteral(c_ulong, n, .decimal);
    }

    pub fn LL_SUFFIX(comptime n: comptime_int) @TypeOf(promoteIntLiteral(c_longlong, n, .decimal)) {
        return promoteIntLiteral(c_longlong, n, .decimal);
    }

    pub fn ULL_SUFFIX(comptime n: comptime_int) @TypeOf(promoteIntLiteral(c_ulonglong, n, .decimal)) {
        return promoteIntLiteral(c_ulonglong, n, .decimal);
    }

    pub fn F_SUFFIX(comptime f: comptime_float) f32 {
        return @as(f32, f);
    }

    pub fn WL_CONTAINER_OF(ptr: anytype, sample: anytype, comptime member: []const u8) @TypeOf(sample) {
        return @fieldParentPtr(@TypeOf(sample.*), member, ptr);
    }

    /// A 2-argument function-like macro defined as #define FOO(A, B) (A)(B)
    /// could be either: cast B to A, or call A with the value B.
    pub fn CAST_OR_CALL(a: anytype, b: anytype) switch (@typeInfo(@TypeOf(a))) {
        .Type => a,
        .Fn => |fn_info| fn_info.return_type orelse void,
        else => |info| @compileError("Unexpected argument type: " ++ @tagName(info)),
    } {
        switch (@typeInfo(@TypeOf(a))) {
            .Type => return cast(a, b),
            .Fn => return a(b),
            else => unreachable, // return type will be a compile error otherwise
        }
    }

    pub inline fn DISCARD(x: anytype) void {
        _ = x;
    }
};

/// Integer promotion described in C11 6.3.1.1.2
fn PromotedIntType(comptime T: type) type {
    return switch (T) {
        bool, u8, i8, c_short => c_int,
        c_ushort => if (@sizeOf(c_ushort) == @sizeOf(c_int)) c_uint else c_int,
        c_int, c_uint, c_long, c_ulong, c_longlong, c_ulonglong => T,
        else => if (T == comptime_int) {
            @compileError("Cannot promote `" ++ @typeName(T) ++ "`; a fixed-size number type is required");
        } else if (@typeInfo(T) == .Int) {
            @compileError("Cannot promote `" ++ @typeName(T) ++ "`; a C ABI type is required");
        } else {
            @compileError("Attempted to promote invalid type `" ++ @typeName(T) ++ "`");
        },
    };
}

/// C11 6.3.1.1.1
fn integerRank(comptime T: type) u8 {
    return switch (T) {
        bool => 0,
        u8, i8 => 1,
        c_short, c_ushort => 2,
        c_int, c_uint => 3,
        c_long, c_ulong => 4,
        c_longlong, c_ulonglong => 5,
        else => @compileError("integer rank not supported for `" ++ @typeName(T) ++ "`"),
    };
}

fn ToUnsigned(comptime T: type) type {
    return switch (T) {
        c_int => c_uint,
        c_long => c_ulong,
        c_longlong => c_ulonglong,
        else => @compileError("Cannot convert `" ++ @typeName(T) ++ "` to unsigned"),
    };
}

/// "Usual arithmetic conversions" from C11 standard 6.3.1.8
fn ArithmeticConversion(comptime A: type, comptime B: type) type {
    if (A == c_longdouble or B == c_longdouble) return c_longdouble;
    if (A == f80 or B == f80) return f80;
    if (A == f64 or B == f64) return f64;
    if (A == f32 or B == f32) return f32;

    const A_Promoted = PromotedIntType(A);
    const B_Promoted = PromotedIntType(B);
    comptime {
        std.assert(integerRank(A_Promoted) >= integerRank(c_int));
        std.assert(integerRank(B_Promoted) >= integerRank(c_int));
    }

    if (A_Promoted == B_Promoted) return A_Promoted;

    const a_signed = @typeInfo(A_Promoted).Int.signedness == .signed;
    const b_signed = @typeInfo(B_Promoted).Int.signedness == .signed;

    if (a_signed == b_signed) {
        return if (integerRank(A_Promoted) > integerRank(B_Promoted)) A_Promoted else B_Promoted;
    }

    const SignedType = if (a_signed) A_Promoted else B_Promoted;
    const UnsignedType = if (!a_signed) A_Promoted else B_Promoted;

    if (integerRank(UnsignedType) >= integerRank(SignedType)) return UnsignedType;

    if (std.math.maxInt(SignedType) >= std.math.maxInt(UnsignedType)) return SignedType;

    return ToUnsigned(SignedType);
}

pub const MacroArithmetic = struct {
    pub fn div(a: anytype, b: anytype) ArithmeticConversion(@TypeOf(a), @TypeOf(b)) {
        const ResType = ArithmeticConversion(@TypeOf(a), @TypeOf(b));
        const a_casted = cast(ResType, a);
        const b_casted = cast(ResType, b);
        switch (@typeInfo(ResType)) {
            .Float => return a_casted / b_casted,
            .Int => return @divTrunc(a_casted, b_casted),
            else => unreachable,
        }
    }

    pub fn rem(a: anytype, b: anytype) ArithmeticConversion(@TypeOf(a), @TypeOf(b)) {
        const ResType = ArithmeticConversion(@TypeOf(a), @TypeOf(b));
        const a_casted = cast(ResType, a);
        const b_casted = cast(ResType, b);
        switch (@typeInfo(ResType)) {
            .Int => {
                if (@typeInfo(ResType).Int.signedness == .signed) {
                    return signedRemainder(a_casted, b_casted);
                } else {
                    return a_casted % b_casted;
                }
            },
            else => unreachable,
        }
    }
};
