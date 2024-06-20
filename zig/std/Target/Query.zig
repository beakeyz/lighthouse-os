//! Contains all the same data as `Target`, additionally introducing the
//! concept of "the native target". The purpose of this abstraction is to
//! provide meaningful and unsurprising defaults. This struct does reference
//! any resources and it is copyable.

/// `null` means native.
cpu_arch: ?Target.Cpu.Arch = null,

cpu_model: CpuModel = CpuModel.determined_by_cpu_arch,

/// Sparse set of CPU features to add to the set from `cpu_model`.
cpu_features_add: Target.Cpu.Feature.Set = Target.Cpu.Feature.Set.empty,

/// Sparse set of CPU features to remove from the set from `cpu_model`.
cpu_features_sub: Target.Cpu.Feature.Set = Target.Cpu.Feature.Set.empty,

/// `null` means native.
os_tag: ?Target.Os.Tag = null,

/// `null` means the default version range for `os_tag`. If `os_tag` is `null` (native)
/// then `null` for this field means native.
os_version_min: ?OsVersion = null,

/// When cross compiling, `null` means default (latest known OS version).
/// When `os_tag` is native, `null` means equal to the native OS version.
os_version_max: ?OsVersion = null,

/// `null` means default when cross compiling, or native when os_tag is native.
/// If `isGnuLibC()` is `false`, this must be `null` and is ignored.
glibc_version: ?SemanticVersion = null,

/// `null` means the native C ABI, if `os_tag` is native, otherwise it means the default C ABI.
abi: ?Target.Abi = null,

/// When `os_tag` is `null`, then `null` means native. Otherwise it means the standard path
/// based on the `os_tag`.
dynamic_linker: Target.DynamicLinker = Target.DynamicLinker.none,

/// `null` means default for the cpu/arch/os combo.
ofmt: ?Target.ObjectFormat = null,

pub const CpuModel = union(enum) {
    /// Always native
    native,

    /// Always baseline
    baseline,

    /// If CPU Architecture is native, then the CPU model will be native. Otherwise,
    /// it will be baseline.
    determined_by_cpu_arch,

    explicit: *const Target.Cpu.Model,

    pub fn eql(a: CpuModel, b: CpuModel) bool {
        const Tag = @typeInfo(CpuModel).Union.tag_type.?;
        const a_tag: Tag = a;
        const b_tag: Tag = b;
        if (a_tag != b_tag) return false;
        return switch (a) {
            .native, .baseline, .determined_by_cpu_arch => true,
            .explicit => |a_model| a_model == b.explicit,
        };
    }
};

pub const OsVersion = union(enum) {
    none: void,
    semver: SemanticVersion,
    windows: Target.Os.WindowsVersion,

    pub fn eql(a: OsVersion, b: OsVersion) bool {
        const Tag = @typeInfo(OsVersion).Union.tag_type.?;
        const a_tag: Tag = a;
        const b_tag: Tag = b;
        if (a_tag != b_tag) return false;
        return switch (a) {
            .none => true,
            .semver => |a_semver| a_semver.order(b.semver) == .eq,
            .windows => |a_windows| a_windows == b.windows,
        };
    }

    pub fn eqlOpt(a: ?OsVersion, b: ?OsVersion) bool {
        if (a == null and b == null) return true;
        if (a == null or b == null) return false;
        return OsVersion.eql(a.?, b.?);
    }
};

pub const SemanticVersion = std.SemanticVersion;

pub fn fromTarget(target: Target) Query {
    var result: Query = .{
        .cpu_arch = target.cpu.arch,
        .cpu_model = .{ .explicit = target.cpu.model },
        .os_tag = target.os.tag,
        .os_version_min = undefined,
        .os_version_max = undefined,
        .abi = target.abi,
        .glibc_version = if (target.isGnuLibC())
            target.os.version_range.linux.glibc
        else
            null,
    };
    result.updateOsVersionRange(target.os);

    const all_features = target.cpu.arch.allFeaturesList();
    var cpu_model_set = target.cpu.model.features;
    cpu_model_set.populateDependencies(all_features);
    {
        // The "add" set is the full set with the CPU Model set removed.
        const add_set = &result.cpu_features_add;
        add_set.* = target.cpu.features;
        add_set.removeFeatureSet(cpu_model_set);
    }
    {
        // The "sub" set is the features that are on in CPU Model set and off in the full set.
        const sub_set = &result.cpu_features_sub;
        sub_set.* = cpu_model_set;
        sub_set.removeFeatureSet(target.cpu.features);
    }
    return result;
}

fn updateOsVersionRange(self: *Query, os: Target.Os) void {
    switch (os.tag) {
        .freestanding,
        .ananas,
        .cloudabi,
        .fuchsia,
        .kfreebsd,
        .lv2,
        .solaris,
        .illumos,
        .zos,
        .haiku,
        .minix,
        .rtems,
        .nacl,
        .aix,
        .cuda,
        .nvcl,
        .amdhsa,
        .ps4,
        .ps5,
        .elfiamcu,
        .mesa3d,
        .contiki,
        .amdpal,
        .hermit,
        .hurd,
        .wasi,
        .emscripten,
        .driverkit,
        .shadermodel,
        .liteos,
        .uefi,
        .opencl,
        .glsl450,
        .vulkan,
        .plan9,
        .other,
        => {
            self.os_version_min = .{ .none = {} };
            self.os_version_max = .{ .none = {} };
        },

        .freebsd,
        .macos,
        .ios,
        .tvos,
        .watchos,
        .netbsd,
        .openbsd,
        .dragonfly,
        => {
            self.os_version_min = .{ .semver = os.version_range.semver.min };
            self.os_version_max = .{ .semver = os.version_range.semver.max };
        },

        .linux => {
            self.os_version_min = .{ .semver = os.version_range.linux.range.min };
            self.os_version_max = .{ .semver = os.version_range.linux.range.max };
        },

        .windows => {
            self.os_version_min = .{ .windows = os.version_range.windows.min };
            self.os_version_max = .{ .windows = os.version_range.windows.max };
        },
    }
}

pub const ParseOptions = struct {
    /// This is sometimes called a "triple". It looks roughly like this:
    ///     riscv64-linux-musl
    /// The fields are, respectively:
    /// * CPU Architecture
    /// * Operating System (and optional version range)
    /// * C ABI (optional, with optional glibc version)
    /// The string "native" can be used for CPU architecture as well as Operating System.
    /// If the CPU Architecture is specified as "native", then the Operating System and C ABI may be omitted.
    arch_os_abi: []const u8 = "native",

    /// Looks like "name+a+b-c-d+e", where "name" is a CPU Model name, "a", "b", and "e"
    /// are examples of CPU features to add to the set, and "c" and "d" are examples of CPU features
    /// to remove from the set.
    /// The following special strings are recognized for CPU Model name:
    /// * "baseline" - The "default" set of CPU features for cross-compiling. A conservative set
    ///                of features that is expected to be supported on most available hardware.
    /// * "native"   - The native CPU model is to be detected when compiling.
    /// If this field is not provided (`null`), then the value will depend on the
    /// parsed CPU Architecture. If native, then this will be "native". Otherwise, it will be "baseline".
    cpu_features: ?[]const u8 = null,

    /// Absolute path to dynamic linker, to override the default, which is either a natively
    /// detected path, or a standard path.
    dynamic_linker: ?[]const u8 = null,

    object_format: ?[]const u8 = null,

    /// If this is provided, the function will populate some information about parsing failures,
    /// so that user-friendly error messages can be delivered.
    diagnostics: ?*Diagnostics = null,

    pub const Diagnostics = struct {
        /// If the architecture was determined, this will be populated.
        arch: ?Target.Cpu.Arch = null,

        /// If the OS name was determined, this will be populated.
        os_name: ?[]const u8 = null,

        /// If the OS tag was determined, this will be populated.
        os_tag: ?Target.Os.Tag = null,

        /// If the ABI was determined, this will be populated.
        abi: ?Target.Abi = null,

        /// If the CPU name was determined, this will be populated.
        cpu_name: ?[]const u8 = null,

        /// If error.UnknownCpuFeature is returned, this will be populated.
        unknown_feature_name: ?[]const u8 = null,
    };
};

pub fn parse(args: ParseOptions) !Query {
    var dummy_diags: ParseOptions.Diagnostics = undefined;
    const diags = args.diagnostics orelse &dummy_diags;

    var result: Query = .{
        .dynamic_linker = Target.DynamicLinker.init(args.dynamic_linker),
    };

    var it = mem.splitScalar(u8, args.arch_os_abi, '-');
    const arch_name = it.first();
    const arch_is_native = mem.eql(u8, arch_name, "native");
    if (!arch_is_native) {
        result.cpu_arch = std.meta.stringToEnum(Target.Cpu.Arch, arch_name) orelse
            return error.UnknownArchitecture;
    }
    const arch = result.cpu_arch orelse builtin.cpu.arch;
    diags.arch = arch;

    if (it.next()) |os_text| {
        try parseOs(&result, diags, os_text);
    } else if (!arch_is_native) {
        return error.MissingOperatingSystem;
    }

    const opt_abi_text = it.next();
    if (opt_abi_text) |abi_text| {
        var abi_it = mem.splitScalar(u8, abi_text, '.');
        const abi = std.meta.stringToEnum(Target.Abi, abi_it.first()) orelse
            return error.UnknownApplicationBinaryInterface;
        result.abi = abi;
        diags.abi = abi;

        const abi_ver_text = abi_it.rest();
        if (abi_it.next() != null) {
            if (Target.isGnuLibC_os_tag_abi(result.os_tag orelse builtin.os.tag, abi)) {
                result.glibc_version = parseVersion(abi_ver_text) catch |err| switch (err) {
                    error.Overflow => return error.InvalidAbiVersion,
                    error.InvalidVersion => return error.InvalidAbiVersion,
                };
            } else {
                return error.InvalidAbiVersion;
            }
        }
    }

    if (it.next() != null) return error.UnexpectedExtraField;

    if (args.cpu_features) |cpu_features| {
        const all_features = arch.allFeaturesList();
        var index: usize = 0;
        while (index < cpu_features.len and
            cpu_features[index] != '+' and
            cpu_features[index] != '-')
        {
            index += 1;
        }
        const cpu_name = cpu_features[0..index];
        diags.cpu_name = cpu_name;

        const add_set = &result.cpu_features_add;
        const sub_set = &result.cpu_features_sub;
        if (mem.eql(u8, cpu_name, "native")) {
            result.cpu_model = .native;
        } else if (mem.eql(u8, cpu_name, "baseline")) {
            result.cpu_model = .baseline;
        } else {
            result.cpu_model = .{ .explicit = try arch.parseCpuModel(cpu_name) };
        }

        while (index < cpu_features.len) {
            const op = cpu_features[index];
            const set = switch (op) {
                '+' => add_set,
                '-' => sub_set,
                else => unreachable,
            };
            index += 1;
            const start = index;
            while (index < cpu_features.len and
                cpu_features[index] != '+' and
                cpu_features[index] != '-')
            {
                index += 1;
            }
            const feature_name = cpu_features[start..index];
            for (all_features, 0..) |feature, feat_index_usize| {
                const feat_index = @as(Target.Cpu.Feature.Set.Index, @intCast(feat_index_usize));
                if (mem.eql(u8, feature_name, feature.name)) {
                    set.addFeature(feat_index);
                    break;
                }
            } else {
                diags.unknown_feature_name = feature_name;
                return error.UnknownCpuFeature;
            }
        }
    }

    if (args.object_format) |ofmt_name| {
        result.ofmt = std.meta.stringToEnum(Target.ObjectFormat, ofmt_name) orelse
            return error.UnknownObjectFormat;
    }

    return result;
}

/// Similar to `parse` except instead of fully parsing, it only determines the CPU
/// architecture and returns it if it can be determined, and returns `null` otherwise.
/// This is intended to be used if the API user of Query needs to learn the
/// target CPU architecture in order to fully populate `ParseOptions`.
pub fn parseCpuArch(args: ParseOptions) ?Target.Cpu.Arch {
    var it = mem.splitScalar(u8, args.arch_os_abi, '-');
    const arch_name = it.first();
    const arch_is_native = mem.eql(u8, arch_name, "native");
    if (arch_is_native) {
        return builtin.cpu.arch;
    } else {
        return std.meta.stringToEnum(Target.Cpu.Arch, arch_name);
    }
}

/// Similar to `SemanticVersion.parse`, but with following changes:
/// * Leading zeroes are allowed.
/// * Supports only 2 or 3 version components (major, minor, [patch]). If 3-rd component is omitted, it will be 0.
pub fn parseVersion(ver: []const u8) error{ InvalidVersion, Overflow }!SemanticVersion {
    const parseVersionComponentFn = (struct {
        fn parseVersionComponentInner(component: []const u8) error{ InvalidVersion, Overflow }!usize {
            return std.fmt.parseUnsigned(usize, component, 10) catch |err| switch (err) {
                error.InvalidCharacter => return error.InvalidVersion,
                error.Overflow => return error.Overflow,
            };
        }
    }).parseVersionComponentInner;
    var version_components = mem.splitScalar(u8, ver, '.');
    const major = version_components.first();
    const minor = version_components.next() orelse return error.InvalidVersion;
    const patch = version_components.next() orelse "0";
    if (version_components.next() != null) return error.InvalidVersion;
    return .{
        .major = try parseVersionComponentFn(major),
        .minor = try parseVersionComponentFn(minor),
        .patch = try parseVersionComponentFn(patch),
    };
}

pub fn isNativeCpu(self: Query) bool {
    return self.cpu_arch == null and
        (self.cpu_model == .native or self.cpu_model == .determined_by_cpu_arch) and
        self.cpu_features_sub.isEmpty() and self.cpu_features_add.isEmpty();
}

pub fn isNativeOs(self: Query) bool {
    return self.os_tag == null and self.os_version_min == null and self.os_version_max == null and
        self.dynamic_linker.get() == null and self.glibc_version == null;
}

pub fn isNativeAbi(self: Query) bool {
    return self.os_tag == null and self.abi == null;
}

pub fn isNative(self: Query) bool {
    return self.isNativeCpu() and self.isNativeOs() and self.isNativeAbi();
}

/// Formats a version with the patch component omitted if it is zero,
/// unlike SemanticVersion.format which formats all its version components regardless.
fn formatVersion(version: SemanticVersion, writer: anytype) !void {
    if (version.patch == 0) {
        try writer.print("{d}.{d}", .{ version.major, version.minor });
    } else {
        try writer.print("{d}.{d}.{d}", .{ version.major, version.minor, version.patch });
    }
}

pub fn zigTriple(self: Query, allocator: Allocator) Allocator.Error![]u8 {
    if (self.isNative()) {
        return allocator.dupe(u8, "native");
    }

    const arch_name = if (self.cpu_arch) |arch| @tagName(arch) else "native";
    const os_name = if (self.os_tag) |os_tag| @tagName(os_tag) else "native";

    var result = std.ArrayList(u8).init(allocator);
    defer result.deinit();

    try result.writer().print("{s}-{s}", .{ arch_name, os_name });

    // The zig target syntax does not allow specifying a max os version with no min, so
    // if either are present, we need the min.
    if (self.os_version_min) |min| {
        switch (min) {
            .none => {},
            .semver => |v| {
                try result.writer().writeAll(".");
                try formatVersion(v, result.writer());
            },
            .windows => |v| {
                try result.writer().print("{s}", .{v});
            },
        }
    }
    if (self.os_version_max) |max| {
        switch (max) {
            .none => {},
            .semver => |v| {
                try result.writer().writeAll("...");
                try formatVersion(v, result.writer());
            },
            .windows => |v| {
                // This is counting on a custom format() function defined on `WindowsVersion`
                // to add a prefix '.' and make there be a total of three dots.
                try result.writer().print("..{s}", .{v});
            },
        }
    }

    if (self.glibc_version) |v| {
        const name = if (self.abi) |abi| @tagName(abi) else "gnu";
        try result.ensureUnusedCapacity(name.len + 2);
        result.appendAssumeCapacity('-');
        result.appendSliceAssumeCapacity(name);
        result.appendAssumeCapacity('.');
        try formatVersion(v, result.writer());
    } else if (self.abi) |abi| {
        const name = @tagName(abi);
        try result.ensureUnusedCapacity(name.len + 1);
        result.appendAssumeCapacity('-');
        result.appendSliceAssumeCapacity(name);
    }

    return result.toOwnedSlice();
}

/// Renders the query into a textual representation that can be parsed via the
/// `-mcpu` flag passed to the Zig compiler.
/// Appends the result to `buffer`.
pub fn serializeCpu(q: Query, buffer: *std.ArrayList(u8)) Allocator.Error!void {
    try buffer.ensureUnusedCapacity(8);
    switch (q.cpu_model) {
        .native => {
            buffer.appendSliceAssumeCapacity("native");
        },
        .baseline => {
            buffer.appendSliceAssumeCapacity("baseline");
        },
        .determined_by_cpu_arch => {
            if (q.cpu_arch == null) {
                buffer.appendSliceAssumeCapacity("native");
            } else {
                buffer.appendSliceAssumeCapacity("baseline");
            }
        },
        .explicit => |model| {
            try buffer.appendSlice(model.name);
        },
    }

    if (q.cpu_features_add.isEmpty() and q.cpu_features_sub.isEmpty()) {
        // The CPU name alone is sufficient.
        return;
    }

    const cpu_arch = q.cpu_arch orelse builtin.cpu.arch;
    const all_features = cpu_arch.allFeaturesList();

    for (all_features, 0..) |feature, i_usize| {
        const i: Target.Cpu.Feature.Set.Index = @intCast(i_usize);
        try buffer.ensureUnusedCapacity(feature.name.len + 1);
        if (q.cpu_features_sub.isEnabled(i)) {
            buffer.appendAssumeCapacity('-');
            buffer.appendSliceAssumeCapacity(feature.name);
        } else if (q.cpu_features_add.isEnabled(i)) {
            buffer.appendAssumeCapacity('+');
            buffer.appendSliceAssumeCapacity(feature.name);
        }
    }
}

pub fn serializeCpuAlloc(q: Query, ally: Allocator) Allocator.Error![]u8 {
    var buffer = std.ArrayList(u8).init(ally);
    try serializeCpu(q, &buffer);
    return buffer.toOwnedSlice();
}

pub fn allocDescription(self: Query, allocator: Allocator) ![]u8 {
    // TODO is there anything else worthy of the description that is not
    // already captured in the triple?
    return self.zigTriple(allocator);
}

pub fn setGnuLibCVersion(self: *Query, major: u32, minor: u32, patch: u32) void {
    self.glibc_version = SemanticVersion{ .major = major, .minor = minor, .patch = patch };
}

fn parseOs(result: *Query, diags: *ParseOptions.Diagnostics, text: []const u8) !void {
    var it = mem.splitScalar(u8, text, '.');
    const os_name = it.first();
    diags.os_name = os_name;
    const os_is_native = mem.eql(u8, os_name, "native");
    if (!os_is_native) {
        result.os_tag = std.meta.stringToEnum(Target.Os.Tag, os_name) orelse
            return error.UnknownOperatingSystem;
    }
    const tag = result.os_tag orelse builtin.os.tag;
    diags.os_tag = tag;

    const version_text = it.rest();
    if (it.next() == null) return;

    switch (tag) {
        .freestanding,
        .ananas,
        .cloudabi,
        .fuchsia,
        .kfreebsd,
        .lv2,
        .solaris,
        .illumos,
        .zos,
        .haiku,
        .minix,
        .rtems,
        .nacl,
        .aix,
        .cuda,
        .nvcl,
        .amdhsa,
        .ps4,
        .ps5,
        .elfiamcu,
        .mesa3d,
        .contiki,
        .amdpal,
        .hermit,
        .hurd,
        .wasi,
        .emscripten,
        .uefi,
        .opencl,
        .glsl450,
        .vulkan,
        .plan9,
        .driverkit,
        .shadermodel,
        .liteos,
        .other,
        => return error.InvalidOperatingSystemVersion,

        .freebsd,
        .macos,
        .ios,
        .tvos,
        .watchos,
        .netbsd,
        .openbsd,
        .linux,
        .dragonfly,
        => {
            var range_it = mem.splitSequence(u8, version_text, "...");

            const min_text = range_it.next().?;
            const min_ver = parseVersion(min_text) catch |err| switch (err) {
                error.Overflow => return error.InvalidOperatingSystemVersion,
                error.InvalidVersion => return error.InvalidOperatingSystemVersion,
            };
            result.os_version_min = .{ .semver = min_ver };

            const max_text = range_it.next() orelse return;
            const max_ver = parseVersion(max_text) catch |err| switch (err) {
                error.Overflow => return error.InvalidOperatingSystemVersion,
                error.InvalidVersion => return error.InvalidOperatingSystemVersion,
            };
            result.os_version_max = .{ .semver = max_ver };
        },

        .windows => {
            var range_it = mem.splitSequence(u8, version_text, "...");

            const min_text = range_it.first();
            const min_ver = std.meta.stringToEnum(Target.Os.WindowsVersion, min_text) orelse
                return error.InvalidOperatingSystemVersion;
            result.os_version_min = .{ .windows = min_ver };

            const max_text = range_it.next() orelse return;
            const max_ver = std.meta.stringToEnum(Target.Os.WindowsVersion, max_text) orelse
                return error.InvalidOperatingSystemVersion;
            result.os_version_max = .{ .windows = max_ver };
        },
    }
}

pub fn eql(a: Query, b: Query) bool {
    if (a.cpu_arch != b.cpu_arch) return false;
    if (!a.cpu_model.eql(b.cpu_model)) return false;
    if (!a.cpu_features_add.eql(b.cpu_features_add)) return false;
    if (!a.cpu_features_sub.eql(b.cpu_features_sub)) return false;
    if (a.os_tag != b.os_tag) return false;
    if (!OsVersion.eqlOpt(a.os_version_min, b.os_version_min)) return false;
    if (!OsVersion.eqlOpt(a.os_version_max, b.os_version_max)) return false;
    if (!versionEqualOpt(a.glibc_version, b.glibc_version)) return false;
    if (a.abi != b.abi) return false;
    if (!a.dynamic_linker.eql(b.dynamic_linker)) return false;
    if (a.ofmt != b.ofmt) return false;

    return true;
}

fn versionEqualOpt(a: ?SemanticVersion, b: ?SemanticVersion) bool {
    if (a == null and b == null) return true;
    if (a == null or b == null) return false;
    return SemanticVersion.order(a.?, b.?) == .eq;
}

const Query = @This();
const std = @import("../std.zig");
const builtin = @import("builtin");
const assert = std.assert;
const Target = std.Target;
const mem = std.mem;
const Allocator = std.mem.Allocator;
