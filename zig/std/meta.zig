const std = @import("std.zig");
const mem = std.mem;
const math = std.math;

const Type = std.builtin.Type;

/// Returns the variant of an enum type, `T`, which is named `str`, or `null` if no such variant exists.
pub fn stringToEnum(comptime T: type, str: []const u8) ?T {
    // Using ComptimeStringMap here is more performant, but it will start to take too
    // long to compile if the enum is large enough, due to the current limits of comptime
    // performance when doing things like constructing lookup maps at comptime.
    // TODO The '100' here is arbitrary and should be increased when possible:
    // - https://github.com/ziglang/zig/issues/4055
    // - https://github.com/ziglang/zig/issues/3863
    if (@typeInfo(T).Enum.fields.len <= 100) {
        const kvs = comptime build_kvs: {
            const EnumKV = struct { []const u8, T };
            var kvs_array: [@typeInfo(T).Enum.fields.len]EnumKV = undefined;
            for (@typeInfo(T).Enum.fields, 0..) |enumField, i| {
                kvs_array[i] = .{ enumField.name, @field(T, enumField.name) };
            }
            break :build_kvs kvs_array[0..];
        };
        const map = std.ComptimeStringMap(T, kvs);
        return map.get(str);
    } else {
        inline for (@typeInfo(T).Enum.fields) |enumField| {
            if (mem.eql(u8, str, enumField.name)) {
                return @field(T, enumField.name);
            }
        }
        return null;
    }
}

/// Returns the alignment of type T.
/// Note that if T is a pointer or function type the result is different than
/// the one returned by @alignOf(T).
/// If T is a pointer type the alignment of the type it points to is returned.
/// If T is a function type the alignment a target-dependent value is returned.
pub fn alignment(comptime T: type) comptime_int {
    return switch (@typeInfo(T)) {
        .Optional => |info| switch (@typeInfo(info.child)) {
            .Pointer, .Fn => alignment(info.child),
            else => @alignOf(T),
        },
        .Pointer => |info| info.alignment,
        .Fn => |info| info.alignment,
        else => @alignOf(T),
    };
}

/// Given a parameterized type (array, vector, pointer, optional), returns the "child type".
pub fn Child(comptime T: type) type {
    return switch (@typeInfo(T)) {
        .Array => |info| info.child,
        .Vector => |info| info.child,
        .Pointer => |info| info.child,
        .Optional => |info| info.child,
        else => @compileError("Expected pointer, optional, array or vector type, found '" ++ @typeName(T) ++ "'"),
    };
}

/// Given a "memory span" type (array, slice, vector, or pointer to such), returns the "element type".
pub fn Elem(comptime T: type) type {
    switch (@typeInfo(T)) {
        .Array => |info| return info.child,
        .Vector => |info| return info.child,
        .Pointer => |info| switch (info.size) {
            .One => switch (@typeInfo(info.child)) {
                .Array => |array_info| return array_info.child,
                .Vector => |vector_info| return vector_info.child,
                else => {},
            },
            .Many, .C, .Slice => return info.child,
        },
        .Optional => |info| return Elem(info.child),
        else => {},
    }
    @compileError("Expected pointer, slice, array or vector type, found '" ++ @typeName(T) ++ "'");
}

/// Given a type which can have a sentinel e.g. `[:0]u8`, returns the sentinel value,
/// or `null` if there is not one.
/// Types which cannot possibly have a sentinel will be a compile error.
/// Result is always comptime-known.
pub inline fn sentinel(comptime T: type) ?Elem(T) {
    switch (@typeInfo(T)) {
        .Array => |info| {
            const sentinel_ptr = info.sentinel orelse return null;
            return @as(*const info.child, @ptrCast(sentinel_ptr)).*;
        },
        .Pointer => |info| {
            switch (info.size) {
                .Many, .Slice => {
                    const sentinel_ptr = info.sentinel orelse return null;
                    return @as(*align(1) const info.child, @ptrCast(sentinel_ptr)).*;
                },
                .One => switch (@typeInfo(info.child)) {
                    .Array => |array_info| {
                        const sentinel_ptr = array_info.sentinel orelse return null;
                        return @as(*align(1) const array_info.child, @ptrCast(sentinel_ptr)).*;
                    },
                    else => {},
                },
                else => {},
            }
        },
        else => {},
    }
    @compileError("type '" ++ @typeName(T) ++ "' cannot possibly have a sentinel");
}

/// Given a "memory span" type, returns the same type except with the given sentinel value.
pub fn Sentinel(comptime T: type, comptime sentinel_val: Elem(T)) type {
    switch (@typeInfo(T)) {
        .Pointer => |info| switch (info.size) {
            .One => switch (@typeInfo(info.child)) {
                .Array => |array_info| return @Type(.{
                    .Pointer = .{
                        .size = info.size,
                        .is_const = info.is_const,
                        .is_volatile = info.is_volatile,
                        .alignment = info.alignment,
                        .address_space = info.address_space,
                        .child = @Type(.{
                            .Array = .{
                                .len = array_info.len,
                                .child = array_info.child,
                                .sentinel = @as(?*const anyopaque, @ptrCast(&sentinel_val)),
                            },
                        }),
                        .is_allowzero = info.is_allowzero,
                        .sentinel = info.sentinel,
                    },
                }),
                else => {},
            },
            .Many, .Slice => return @Type(.{
                .Pointer = .{
                    .size = info.size,
                    .is_const = info.is_const,
                    .is_volatile = info.is_volatile,
                    .alignment = info.alignment,
                    .address_space = info.address_space,
                    .child = info.child,
                    .is_allowzero = info.is_allowzero,
                    .sentinel = @as(?*const anyopaque, @ptrCast(&sentinel_val)),
                },
            }),
            else => {},
        },
        .Optional => |info| switch (@typeInfo(info.child)) {
            .Pointer => |ptr_info| switch (ptr_info.size) {
                .Many => return @Type(.{
                    .Optional = .{
                        .child = @Type(.{
                            .Pointer = .{
                                .size = ptr_info.size,
                                .is_const = ptr_info.is_const,
                                .is_volatile = ptr_info.is_volatile,
                                .alignment = ptr_info.alignment,
                                .address_space = ptr_info.address_space,
                                .child = ptr_info.child,
                                .is_allowzero = ptr_info.is_allowzero,
                                .sentinel = @as(?*const anyopaque, @ptrCast(&sentinel_val)),
                            },
                        }),
                    },
                }),
                else => {},
            },
            else => {},
        },
        else => {},
    }
    @compileError("Unable to derive a sentinel pointer type from " ++ @typeName(T));
}

pub fn containerLayout(comptime T: type) Type.ContainerLayout {
    return switch (@typeInfo(T)) {
        .Struct => |info| info.layout,
        .Union => |info| info.layout,
        else => @compileError("expected struct or union type, found '" ++ @typeName(T) ++ "'"),
    };
}

/// Instead of this function, prefer to use e.g. `@typeInfo(foo).Struct.decls`
/// directly when you know what kind of type it is.
pub fn declarations(comptime T: type) []const Type.Declaration {
    return switch (@typeInfo(T)) {
        .Struct => |info| info.decls,
        .Enum => |info| info.decls,
        .Union => |info| info.decls,
        .Opaque => |info| info.decls,
        else => @compileError("Expected struct, enum, union, or opaque type, found '" ++ @typeName(T) ++ "'"),
    };
}

pub fn declarationInfo(comptime T: type, comptime decl_name: []const u8) Type.Declaration {
    inline for (comptime declarations(T)) |decl| {
        if (comptime mem.eql(u8, decl.name, decl_name))
            return decl;
    }

    @compileError("'" ++ @typeName(T) ++ "' has no declaration '" ++ decl_name ++ "'");
}

pub fn fields(comptime T: type) switch (@typeInfo(T)) {
    .Struct => []const Type.StructField,
    .Union => []const Type.UnionField,
    .ErrorSet => []const Type.Error,
    .Enum => []const Type.EnumField,
    else => @compileError("Expected struct, union, error set or enum type, found '" ++ @typeName(T) ++ "'"),
} {
    return switch (@typeInfo(T)) {
        .Struct => |info| info.fields,
        .Union => |info| info.fields,
        .Enum => |info| info.fields,
        .ErrorSet => |errors| errors.?, // must be non global error set
        else => @compileError("Expected struct, union, error set or enum type, found '" ++ @typeName(T) ++ "'"),
    };
}

pub fn fieldInfo(comptime T: type, comptime field: FieldEnum(T)) switch (@typeInfo(T)) {
    .Struct => Type.StructField,
    .Union => Type.UnionField,
    .ErrorSet => Type.Error,
    .Enum => Type.EnumField,
    else => @compileError("Expected struct, union, error set or enum type, found '" ++ @typeName(T) ++ "'"),
} {
    return fields(T)[@intFromEnum(field)];
}

pub fn FieldType(comptime T: type, comptime field: FieldEnum(T)) type {
    if (@typeInfo(T) != .Struct and @typeInfo(T) != .Union) {
        @compileError("Expected struct or union, found '" ++ @typeName(T) ++ "'");
    }

    return fieldInfo(T, field).type;
}

pub fn fieldNames(comptime T: type) *const [fields(T).len][]const u8 {
    return comptime blk: {
        const fieldInfos = fields(T);
        var names: [fieldInfos.len][]const u8 = undefined;
        for (fieldInfos, 0..) |field, i| {
            names[i] = field.name;
        }
        break :blk &names;
    };
}

/// Given an enum or error set type, returns a pointer to an array containing all tags for that
/// enum or error set.
pub fn tags(comptime T: type) *const [fields(T).len]T {
    return comptime blk: {
        const fieldInfos = fields(T);
        var res: [fieldInfos.len]T = undefined;
        for (fieldInfos, 0..) |field, i| {
            res[i] = @field(T, field.name);
        }
        break :blk &res;
    };
}

/// Returns an enum with a variant named after each field of `T`.
pub fn FieldEnum(comptime T: type) type {
    const field_infos = fields(T);

    if (field_infos.len == 0) {
        return @Type(.{
            .Enum = .{
                .tag_type = u0,
                .fields = &.{},
                .decls = &.{},
                .is_exhaustive = true,
            },
        });
    }

    if (@typeInfo(T) == .Union) {
        if (@typeInfo(T).Union.tag_type) |tag_type| {
            for (std.enums.values(tag_type), 0..) |v, i| {
                if (@intFromEnum(v) != i) break; // enum values not consecutive
                if (!std.mem.eql(u8, @tagName(v), field_infos[i].name)) break; // fields out of order
            } else {
                return tag_type;
            }
        }
    }

    var enumFields: [field_infos.len]std.builtin.Type.EnumField = undefined;
    var decls = [_]std.builtin.Type.Declaration{};
    inline for (field_infos, 0..) |field, i| {
        enumFields[i] = .{
            .name = field.name ++ "",
            .value = i,
        };
    }
    return @Type(.{
        .Enum = .{
            .tag_type = std.math.IntFittingRange(0, field_infos.len - 1),
            .fields = &enumFields,
            .decls = &decls,
            .is_exhaustive = true,
        },
    });
}

pub fn DeclEnum(comptime T: type) type {
    const fieldInfos = std.meta.declarations(T);
    var enumDecls: [fieldInfos.len]std.builtin.Type.EnumField = undefined;
    var decls = [_]std.builtin.Type.Declaration{};
    inline for (fieldInfos, 0..) |field, i| {
        enumDecls[i] = .{ .name = field.name ++ "", .value = i };
    }
    return @Type(.{
        .Enum = .{
            .tag_type = std.math.IntFittingRange(0, fieldInfos.len - 1),
            .fields = &enumDecls,
            .decls = &decls,
            .is_exhaustive = true,
        },
    });
}

pub fn Tag(comptime T: type) type {
    return switch (@typeInfo(T)) {
        .Enum => |info| info.tag_type,
        .Union => |info| info.tag_type orelse @compileError(@typeName(T) ++ " has no tag type"),
        else => @compileError("expected enum or union type, found '" ++ @typeName(T) ++ "'"),
    };
}

///Returns the active tag of a tagged union
pub fn activeTag(u: anytype) Tag(@TypeOf(u)) {
    const T = @TypeOf(u);
    return @as(Tag(T), u);
}

const TagPayloadType = TagPayload;

pub fn TagPayloadByName(comptime U: type, comptime tag_name: []const u8) type {
    const info = @typeInfo(U).Union;

    inline for (info.fields) |field_info| {
        if (comptime mem.eql(u8, field_info.name, tag_name))
            return field_info.type;
    }

    unreachable;
}

/// Given a tagged union type, and an enum, return the type of the union field
/// corresponding to the enum tag.
pub fn TagPayload(comptime U: type, comptime tag: Tag(U)) type {
    return TagPayloadByName(U, @tagName(tag));
}

/// Compares two of any type for equality. Containers are compared on a field-by-field basis,
/// where possible. Pointers are not followed.
pub fn eql(a: anytype, b: @TypeOf(a)) bool {
    const T = @TypeOf(a);

    switch (@typeInfo(T)) {
        .Struct => |info| {
            inline for (info.fields) |field_info| {
                if (!eql(@field(a, field_info.name), @field(b, field_info.name))) return false;
            }
            return true;
        },
        .ErrorUnion => {
            if (a) |a_p| {
                if (b) |b_p| return eql(a_p, b_p) else |_| return false;
            } else |a_e| {
                if (b) |_| return false else |b_e| return a_e == b_e;
            }
        },
        .Union => |info| {
            if (info.tag_type) |UnionTag| {
                const tag_a = activeTag(a);
                const tag_b = activeTag(b);
                if (tag_a != tag_b) return false;

                inline for (info.fields) |field_info| {
                    if (@field(UnionTag, field_info.name) == tag_a) {
                        return eql(@field(a, field_info.name), @field(b, field_info.name));
                    }
                }
                return false;
            }

            @compileError("cannot compare untagged union type " ++ @typeName(T));
        },
        .Array => {
            if (a.len != b.len) return false;
            for (a, 0..) |e, i|
                if (!eql(e, b[i])) return false;
            return true;
        },
        .Vector => |info| {
            var i: usize = 0;
            while (i < info.len) : (i += 1) {
                if (!eql(a[i], b[i])) return false;
            }
            return true;
        },
        .Pointer => |info| {
            return switch (info.size) {
                .One, .Many, .C => a == b,
                .Slice => a.ptr == b.ptr and a.len == b.len,
            };
        },
        .Optional => {
            if (a == null and b == null) return true;
            if (a == null or b == null) return false;
            return eql(a.?, b.?);
        },
        else => return a == b,
    }
}

pub const IntToEnumError = error{InvalidEnumTag};

pub fn intToEnum(comptime EnumTag: type, tag_int: anytype) IntToEnumError!EnumTag {
    const enum_info = @typeInfo(EnumTag).Enum;

    if (!enum_info.is_exhaustive) {
        if (std.math.cast(enum_info.tag_type, tag_int)) |tag| {
            return @as(EnumTag, @enumFromInt(tag));
        }
        return error.InvalidEnumTag;
    }

    // We don't direcly iterate over the fields of EnumTag, as that
    // would require an inline loop. Instead, we create an array of
    // values that is comptime-know, but can be iterated at runtime
    // without requiring an inline loop. This generates better
    // machine code.
    const values = comptime blk: {
        var result: [enum_info.fields.len]enum_info.tag_type = undefined;
        for (&result, enum_info.fields) |*dst, src| {
            dst.* = src.value;
        }
        break :blk result;
    };
    for (values) |v| {
        if (v == tag_int) return @enumFromInt(tag_int);
    }
    return error.InvalidEnumTag;
}

/// Given a type and a name, return the field index according to source order.
/// Returns `null` if the field is not found.
pub fn fieldIndex(comptime T: type, comptime name: []const u8) ?comptime_int {
    inline for (fields(T), 0..) |field, i| {
        if (mem.eql(u8, field.name, name))
            return i;
    }
    return null;
}

// Returns a slice of pointers to public declarations of a namespace.
//pub fn declList(comptime Namespace: type, comptime Decl: type) []const *const Decl {
//const S = struct {
//   fn declNameLessThan(context: void, lhs: *const Decl, rhs: *const Decl) bool {
//      _ = context;
//return mem.lessThan(u8, lhs.name, rhs.name);
//}
//};
//comptime {
//const decls = declarations(Namespace);
//var array: [decls.len]*const Decl = undefined;
//for (decls, 0..) |decl, i| {
//array[i] = &@field(Namespace, decl.name);
//}
//mem.sort(*const Decl, &array, {}, S.declNameLessThan);
//return &array;
//}
//}

pub fn Int(comptime signedness: std.builtin.Signedness, comptime bit_count: u16) type {
    return @Type(.{
        .Int = .{
            .signedness = signedness,
            .bits = bit_count,
        },
    });
}

pub fn Float(comptime bit_count: u8) type {
    return @Type(.{
        .Float = .{ .bits = bit_count },
    });
}

/// For a given function type, returns a tuple type which fields will
/// correspond to the argument types.
///
/// Examples:
/// - `ArgsTuple(fn () void)` ⇒ `tuple { }`
/// - `ArgsTuple(fn (a: u32) u32)` ⇒ `tuple { u32 }`
/// - `ArgsTuple(fn (a: u32, b: f16) noreturn)` ⇒ `tuple { u32, f16 }`
pub fn ArgsTuple(comptime Function: type) type {
    const info = @typeInfo(Function);
    if (info != .Fn)
        @compileError("ArgsTuple expects a function type");

    const function_info = info.Fn;
    if (function_info.is_var_args)
        @compileError("Cannot create ArgsTuple for variadic function");

    var argument_field_list: [function_info.params.len]type = undefined;
    inline for (function_info.params, 0..) |arg, i| {
        const T = arg.type orelse @compileError("cannot create ArgsTuple for function with an 'anytype' parameter");
        argument_field_list[i] = T;
    }

    return CreateUniqueTuple(argument_field_list.len, argument_field_list);
}

/// For a given anonymous list of types, returns a new tuple type
/// with those types as fields.
///
/// Examples:
/// - `Tuple(&[_]type {})` ⇒ `tuple { }`
/// - `Tuple(&[_]type {f32})` ⇒ `tuple { f32 }`
/// - `Tuple(&[_]type {f32,u32})` ⇒ `tuple { f32, u32 }`
pub fn Tuple(comptime types: []const type) type {
    return CreateUniqueTuple(types.len, types[0..types.len].*);
}

fn CreateUniqueTuple(comptime N: comptime_int, comptime types: [N]type) type {
    var tuple_fields: [types.len]std.builtin.Type.StructField = undefined;
    inline for (types, 0..) |T, i| {
        @setEvalBranchQuota(10_000);
        var num_buf: [128]u8 = undefined;
        tuple_fields[i] = .{
            .name = std.fmt.bufPrintZ(&num_buf, "{d}", .{i}) catch unreachable,
            .type = T,
            .default_value = null,
            .is_comptime = false,
            .alignment = if (@sizeOf(T) > 0) @alignOf(T) else 0,
        };
    }

    return @Type(.{
        .Struct = .{
            .is_tuple = true,
            .layout = .Auto,
            .decls = &.{},
            .fields = &tuple_fields,
        },
    });
}

const TupleTester = struct {
    fn assertTypeEqual(comptime Expected: type, comptime Actual: type) void {
        if (Expected != Actual)
            @compileError("Expected type " ++ @typeName(Expected) ++ ", but got type " ++ @typeName(Actual));
    }

    fn assertTuple(comptime expected: anytype, comptime Actual: type) void {
        const info = @typeInfo(Actual);
        if (info != .Struct)
            @compileError("Expected struct type");
        if (!info.Struct.is_tuple)
            @compileError("Struct type must be a tuple type");

        const fields_list = std.meta.fields(Actual);
        if (expected.len != fields_list.len)
            @compileError("Argument count mismatch");

        inline for (fields_list, 0..) |fld, i| {
            if (expected[i] != fld.type) {
                @compileError("Field " ++ fld.name ++ " expected to be type " ++ @typeName(expected[i]) ++ ", but was type " ++ @typeName(fld.type));
            }
        }
    }
};

/// Returns whether `error_union` contains an error.
pub fn isError(error_union: anytype) bool {
    return if (error_union) |_| false else |_| true;
}

/// Returns true if a type has a namespace and the namespace contains `name`;
/// `false` otherwise. Result is always comptime-known.
pub inline fn hasFn(comptime T: type, comptime name: []const u8) bool {
    switch (@typeInfo(T)) {
        .Struct, .Union, .Enum, .Opaque => {},
        else => return false,
    }
    if (!@hasDecl(T, name))
        return false;

    return @typeInfo(@TypeOf(@field(T, name))) == .Fn;
}

/// Returns true if a type has a `name` method; `false` otherwise.
/// Result is always comptime-known.
pub inline fn hasMethod(comptime T: type, comptime name: []const u8) bool {
    return switch (@typeInfo(T)) {
        .Pointer => |P| switch (P.size) {
            .One => hasFn(P.child, name),
            .Many, .Slice, .C => false,
        },
        else => hasFn(T, name),
    };
}

/// True if every value of the type `T` has a unique bit pattern representing it.
/// In other words, `T` has no unused bits and no padding.
/// Result is always comptime-known.
pub inline fn hasUniqueRepresentation(comptime T: type) bool {
    return switch (@typeInfo(T)) {
        else => false, // TODO can we know if it's true for some of these types ?

        .AnyFrame,
        .Enum,
        .ErrorSet,
        .Fn,
        => true,

        .Bool => false,

        .Int => |info| @sizeOf(T) * 8 == info.bits,

        .Pointer => |info| info.size != .Slice,

        .Array => |info| hasUniqueRepresentation(info.child),

        .Struct => |info| {
            var sum_size = @as(usize, 0);

            inline for (info.fields) |field| {
                if (!hasUniqueRepresentation(field.type)) return false;
                sum_size += @sizeOf(field.type);
            }

            return @sizeOf(T) == sum_size;
        },

        .Vector => |info| hasUniqueRepresentation(info.child) and
            @sizeOf(T) == @sizeOf(info.child) * info.len,
    };
}
