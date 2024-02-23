pub const __builtin_bswap16 = @import("std").zig.c_builtins.__builtin_bswap16;
pub const __builtin_bswap32 = @import("std").zig.c_builtins.__builtin_bswap32;
pub const __builtin_bswap64 = @import("std").zig.c_builtins.__builtin_bswap64;
pub const __builtin_signbit = @import("std").zig.c_builtins.__builtin_signbit;
pub const __builtin_signbitf = @import("std").zig.c_builtins.__builtin_signbitf;
pub const __builtin_popcount = @import("std").zig.c_builtins.__builtin_popcount;
pub const __builtin_ctz = @import("std").zig.c_builtins.__builtin_ctz;
pub const __builtin_clz = @import("std").zig.c_builtins.__builtin_clz;
pub const __builtin_sqrt = @import("std").zig.c_builtins.__builtin_sqrt;
pub const __builtin_sqrtf = @import("std").zig.c_builtins.__builtin_sqrtf;
pub const __builtin_sin = @import("std").zig.c_builtins.__builtin_sin;
pub const __builtin_sinf = @import("std").zig.c_builtins.__builtin_sinf;
pub const __builtin_cos = @import("std").zig.c_builtins.__builtin_cos;
pub const __builtin_cosf = @import("std").zig.c_builtins.__builtin_cosf;
pub const __builtin_exp = @import("std").zig.c_builtins.__builtin_exp;
pub const __builtin_expf = @import("std").zig.c_builtins.__builtin_expf;
pub const __builtin_exp2 = @import("std").zig.c_builtins.__builtin_exp2;
pub const __builtin_exp2f = @import("std").zig.c_builtins.__builtin_exp2f;
pub const __builtin_log = @import("std").zig.c_builtins.__builtin_log;
pub const __builtin_logf = @import("std").zig.c_builtins.__builtin_logf;
pub const __builtin_log2 = @import("std").zig.c_builtins.__builtin_log2;
pub const __builtin_log2f = @import("std").zig.c_builtins.__builtin_log2f;
pub const __builtin_log10 = @import("std").zig.c_builtins.__builtin_log10;
pub const __builtin_log10f = @import("std").zig.c_builtins.__builtin_log10f;
pub const __builtin_abs = @import("std").zig.c_builtins.__builtin_abs;
pub const __builtin_fabs = @import("std").zig.c_builtins.__builtin_fabs;
pub const __builtin_fabsf = @import("std").zig.c_builtins.__builtin_fabsf;
pub const __builtin_floor = @import("std").zig.c_builtins.__builtin_floor;
pub const __builtin_floorf = @import("std").zig.c_builtins.__builtin_floorf;
pub const __builtin_ceil = @import("std").zig.c_builtins.__builtin_ceil;
pub const __builtin_ceilf = @import("std").zig.c_builtins.__builtin_ceilf;
pub const __builtin_trunc = @import("std").zig.c_builtins.__builtin_trunc;
pub const __builtin_truncf = @import("std").zig.c_builtins.__builtin_truncf;
pub const __builtin_round = @import("std").zig.c_builtins.__builtin_round;
pub const __builtin_roundf = @import("std").zig.c_builtins.__builtin_roundf;
pub const __builtin_strlen = @import("std").zig.c_builtins.__builtin_strlen;
pub const __builtin_strcmp = @import("std").zig.c_builtins.__builtin_strcmp;
pub const __builtin_object_size = @import("std").zig.c_builtins.__builtin_object_size;
pub const __builtin___memset_chk = @import("std").zig.c_builtins.__builtin___memset_chk;
pub const __builtin_memset = @import("std").zig.c_builtins.__builtin_memset;
pub const __builtin___memcpy_chk = @import("std").zig.c_builtins.__builtin___memcpy_chk;
pub const __builtin_memcpy = @import("std").zig.c_builtins.__builtin_memcpy;
pub const __builtin_expect = @import("std").zig.c_builtins.__builtin_expect;
pub const __builtin_nanf = @import("std").zig.c_builtins.__builtin_nanf;
pub const __builtin_huge_valf = @import("std").zig.c_builtins.__builtin_huge_valf;
pub const __builtin_inff = @import("std").zig.c_builtins.__builtin_inff;
pub const __builtin_isnan = @import("std").zig.c_builtins.__builtin_isnan;
pub const __builtin_isinf = @import("std").zig.c_builtins.__builtin_isinf;
pub const __builtin_isinf_sign = @import("std").zig.c_builtins.__builtin_isinf_sign;
pub const __has_builtin = @import("std").zig.c_builtins.__has_builtin;
pub const __builtin_assume = @import("std").zig.c_builtins.__builtin_assume;
pub const __builtin_unreachable = @import("std").zig.c_builtins.__builtin_unreachable;
pub const __builtin_constant_p = @import("std").zig.c_builtins.__builtin_constant_p;
pub const __builtin_mul_overflow = @import("std").zig.c_builtins.__builtin_mul_overflow;
pub const uint_t = c_uint;
pub const @"bool" = u8;
pub const vaddr_t = usize;
pub const paddr_t = usize;
pub const struct___va_list_tag = extern struct {
    gp_offset: c_uint,
    fp_offset: c_uint,
    overflow_arg_area: ?*anyopaque,
    reg_save_area: ?*anyopaque,
};
pub const __builtin_va_list = [1]struct___va_list_tag;
pub const va_list = __builtin_va_list;
pub const FuncPtr = ?*const fn (...) callconv(.C) void;
pub const logger_id_t = u8;
pub const logger_t = extern struct {
    title: [*c]u8,
    flags: u8,
    id: logger_id_t,
    f_logc: ?*const fn (u8) callconv(.C) c_int,
    f_log: ?*const fn ([*c]const u8) callconv(.C) c_int,
    f_logln: ?*const fn ([*c]const u8) callconv(.C) c_int,
};
pub extern fn init_early_logging(...) void;
pub extern fn init_logging(...) void;
pub extern fn register_logger(logger: [*c]logger_t) c_int;
pub extern fn unregister_logger(logger: [*c]logger_t) c_int;
pub extern fn logger_get(id: logger_id_t, buffer: [*c]logger_t) c_int;
pub extern fn logger_scan(title: [*c]u8, buffer: [*c]logger_t) c_int;
pub extern fn logger_swap_priority(old: logger_id_t, new: logger_id_t) c_int;
pub extern fn log(msg: [*c]const u8) void;
pub extern fn logf(fmt: [*c]const u8, ...) void;
pub extern fn logln(msg: [*c]const u8) void;
pub extern fn log_ex(id: logger_id_t, msg: [*c]const u8, args: [*c]struct___va_list_tag, @"type": u8) void;
pub extern fn kputch(c: u8) c_int;
pub extern fn print(msg: [*c]const u8) c_int;
pub extern fn println(msg: [*c]const u8) c_int;
pub extern fn printf(fmt: [*c]const u8, ...) c_int;
pub extern fn vprintf(fmt: [*c]const u8, args: [*c]struct___va_list_tag) c_int;
pub extern fn kdbgf(fmt: [*c]const u8, ...) c_int;
pub extern fn kdbgln(msg: [*c]const u8) c_int;
pub extern fn kdbg(msg: [*c]const u8) c_int;
pub extern fn kdbgc(c: u8) c_int;
pub extern fn kwarnf(fmt: [*c]const u8, ...) c_int;
pub extern fn kwarnln(msg: [*c]const u8) c_int;
pub extern fn kwarn(msg: [*c]const u8) c_int;
pub extern fn kwarnc(c: u8) c_int;
pub const kerror_t = c_int;
pub const struct_kerror_frame = extern struct {
    code: kerror_t,
    ctx: [*c]const u8,
    prev: [*c]struct_kerror_frame,
};
pub const kerror_frame_t = struct_kerror_frame;
pub extern fn init_kerror(...) void;
pub extern fn push_kerror(err: kerror_t, ctx: [*c]const u8) kerror_t;
pub extern fn pop_kerror(frame: [*c]kerror_frame_t) kerror_t;
pub extern fn drain_kerrors(...) void;
pub fn kerror_is_fatal(arg_err: kerror_t) callconv(.C) @"bool" {
    var err = arg_err;
    return @as(@"bool", @intFromBool((@as(c_ulong, @bitCast(@as(c_long, err))) & @as(c_ulong, 2147483648)) == @as(c_ulong, 2147483648)));
}
pub const ANIVA_FAIL: c_int = 0;
pub const ANIVA_WARNING: c_int = 1;
pub const ANIVA_SUCCESS: c_int = 2;
pub const ANIVA_NOMEM: c_int = 3;
pub const ANIVA_NODEV: c_int = 4;
pub const ANIVA_NOCONN: c_int = 5;
pub const ANIVA_FULL: c_int = 6;
pub const ANIVA_EMPTY: c_int = 7;
pub const ANIVA_BUSY: c_int = 8;
pub const ANIVA_INVAL: c_int = 9;
pub const enum__ANIVA_STATUS = c_uint;
pub const ANIVA_STATUS = enum__ANIVA_STATUS;
pub fn status_is_ok(arg_status: ANIVA_STATUS) callconv(.C) @"bool" {
    var status = arg_status;
    return @as(@"bool", @intFromBool(status == @as(c_uint, @bitCast(ANIVA_SUCCESS))));
}
pub const struct__ErrorOrPtr = extern struct {
    m_status: ANIVA_STATUS,
    m_ptr: usize,
};
pub const ErrorOrPtr = struct__ErrorOrPtr;
pub extern fn __kernel_panic(...) noreturn;
pub extern fn kernel_panic(panic_message: [*c]const u8) noreturn;
pub inline fn Error() ErrorOrPtr {
    var e: ErrorOrPtr = ErrorOrPtr{
        .m_status = @as(c_uint, @bitCast(ANIVA_FAIL)),
        .m_ptr = @as(usize, @bitCast(@as(c_longlong, @as(c_int, 0)))),
    };
    return e;
}
pub inline fn ErrorWithVal(arg_val: usize) ErrorOrPtr {
    var val = arg_val;
    var e: ErrorOrPtr = ErrorOrPtr{
        .m_status = @as(c_uint, @bitCast(ANIVA_FAIL)),
        .m_ptr = val,
    };
    return e;
}
pub inline fn IsError(arg_e: ErrorOrPtr) @"bool" {
    var e = arg_e;
    return @as(@"bool", @intFromBool(e.m_status == @as(c_uint, @bitCast(ANIVA_FAIL))));
}
pub inline fn Warning() ErrorOrPtr {
    var e: ErrorOrPtr = ErrorOrPtr{
        .m_status = @as(c_uint, @bitCast(ANIVA_WARNING)),
        .m_ptr = @as(usize, @bitCast(@as(c_longlong, @as(c_int, 0)))),
    };
    return e;
}
pub inline fn Success(arg_value: usize) ErrorOrPtr {
    var value = arg_value;
    var e: ErrorOrPtr = ErrorOrPtr{
        .m_status = @as(c_uint, @bitCast(ANIVA_SUCCESS)),
        .m_ptr = value,
    };
    return e;
}
pub inline fn Release(arg_eop: ErrorOrPtr) usize {
    var eop = arg_eop;
    if (eop.m_status == @as(c_uint, @bitCast(ANIVA_SUCCESS))) {
        return eop.m_ptr;
    }
    return 0;
}
pub inline fn Must(arg_eop: ErrorOrPtr) usize {
    var eop = arg_eop;
    if (eop.m_status != @as(c_uint, @bitCast(ANIVA_SUCCESS))) {
        kernel_panic("ErrorOrPtr: Must(...) failed!");
    }
    return eop.m_ptr;
}
pub inline fn GetStatus(arg_eop: ErrorOrPtr) ANIVA_STATUS {
    var eop = arg_eop;
    return eop.m_status;
}
pub const struct_node = extern struct {
    data: ?*anyopaque,
    prev: [*c]struct_node,
    next: [*c]struct_node,
};
pub const node_t = struct_node;
pub const struct_list = extern struct {
    head: [*c]node_t,
    end: [*c]node_t,
    m_length: usize,
};
pub const list_t = struct_list;
pub extern fn init_list(...) [*c]list_t;
pub extern fn create_list(...) list_t;
pub extern fn destroy_list([*c]list_t) void;
pub extern fn list_append([*c]list_t, ?*anyopaque) void;
pub extern fn list_prepend(list: [*c]list_t, data: ?*anyopaque) void;
pub extern fn list_append_before([*c]list_t, ?*anyopaque, u32) void;
pub extern fn list_append_after([*c]list_t, ?*anyopaque, u32) void;
pub extern fn list_remove([*c]list_t, u32) @"bool";
pub extern fn list_remove_ex([*c]list_t, ?*anyopaque) @"bool";
pub extern fn list_get([*c]list_t, u32) ?*anyopaque;
pub extern fn list_indexof(list: [*c]list_t, data: ?*anyopaque) ErrorOrPtr;
pub const hive_url_part_t = [*c]u8;
pub const struct_hive = extern struct {
    m_entries: [*c]list_t,
    m_hole_count: usize,
    m_total_entry_count: usize,
    m_url_part: hive_url_part_t,
};
pub const struct_hive_entry = extern struct {
    m_data: ?*anyopaque,
    m_hole: [*c]struct_hive,
    m_entry_part: hive_url_part_t,
};
pub const HIVE_ENTRY_TYPE_DATA: c_int = 0;
pub const HIVE_ENTRY_TYPE_HOLE: c_int = 1;
pub const enum_HIVE_ENTRY_TYPE = c_uint;
pub const HIVE_ENTRY_TYPE_t = enum_HIVE_ENTRY_TYPE;
pub const hive_entry_t = struct_hive_entry;
pub const hive_t = struct_hive;
pub extern fn create_hive(root_part: hive_url_part_t) [*c]hive_t;
pub extern fn destroy_hive(hive: [*c]hive_t) void;
pub extern fn hive_set(root: [*c]hive_t, data: ?*anyopaque, path: [*c]const u8) void;
pub extern fn hive_add_entry(root: [*c]hive_t, data: ?*anyopaque, path: [*c]const u8) ErrorOrPtr;
pub extern fn hive_add_hole(root: [*c]hive_t, path: [*c]const u8, hole: [*c]hive_t) ErrorOrPtr;
pub extern fn hive_add_holes(root: [*c]hive_t, path: [*c]const u8) ErrorOrPtr;
pub extern fn hive_get(root: [*c]hive_t, path: [*c]const u8) ?*anyopaque;
pub extern fn hive_remove(root: [*c]hive_t, data: ?*anyopaque) ErrorOrPtr;
pub extern fn hive_remove_path(root: [*c]hive_t, path: [*c]const u8) ErrorOrPtr;
pub extern fn hive_get_path(root: [*c]hive_t, data: ?*anyopaque) [*c]const u8;
pub extern fn hive_contains(root: [*c]hive_t, data: ?*anyopaque) @"bool";
pub extern fn hive_get_relative(root: [*c]hive_t, subpath: [*c]const u8) ?*anyopaque;
pub extern fn hive_walk(root: [*c]hive_t, recursive: @"bool", itterate_fn: ?*const fn ([*c]hive_t, ?*anyopaque) callconv(.C) @"bool") ErrorOrPtr;
pub inline fn hive_entry_is_hole(arg_entry: [*c]hive_entry_t) @"bool" {
    var entry = arg_entry;
    return @as(@"bool", @intFromBool(entry.*.m_hole != @as([*c]struct_hive, @ptrCast(@alignCast(@as(?*anyopaque, @ptrFromInt(@as(c_int, 0))))))));
}
pub const struct_packet_response = extern struct {
    m_response_buffer: ?*anyopaque,
    m_response_size: usize,
};
pub const packet_response_t = struct_packet_response;
pub extern fn init_packet_response(...) void;
pub extern fn create_packet_response(data: ?*anyopaque, size: usize) [*c]packet_response_t;
pub extern fn destroy_packet_response(response: [*c]packet_response_t) void;
pub const struct_oss_obj = opaque {};
pub const struct_oss_node = opaque {};
const struct_unnamed_1 = extern struct {
    maj: u8,
    min: u8,
    bump: u16,
};
pub const union_driver_version = extern union {
    unnamed_0: struct_unnamed_1,
    version: u32,
};
pub const driver_version_t = union_driver_version;
pub const dev_type_t = u8;
pub const drv_precedence_t = u8;
pub const driver_control_code_t = u32;
pub const aniva_driver_t = struct_aniva_driver;
pub const struct_vector = extern struct {
    m_capacity: usize,
    m_length: usize,
    m_flags: u16,
    m_entry_size: u16,
    m_grow_size: u32,
    m_items: [*c]u8,
};
pub const vector_t = struct_vector;
pub const kresource_type_t = u16;
pub const kresource_flags_t = u16;
pub const flat_refc_t = c_int;
pub const struct_kresource = extern struct {
    m_name: [*c]const u8,
    f_release: ?*const fn ([*c]struct_kresource) callconv(.C) c_int,
    m_start: usize,
    m_size: usize,
    m_type: kresource_type_t,
    m_flags: kresource_flags_t,
    m_shared_count: flat_refc_t,
    m_owner: ?*anyopaque,
    m_next: [*c]struct_kresource,
};
pub const kresource_t = struct_kresource;
pub const struct_kresource_bundle = extern struct {
    page_dir: [*c]page_dir_t,
    resources: [3][*c]kresource_t,
};
pub const kresource_bundle_t = struct_kresource_bundle;
pub const ThreadEntry = ?*const fn (usize) callconv(.C) c_int;
pub const ThreadEntryWrapper = ?*const fn (usize, [*c]struct_thread) callconv(.C) void;
pub const thread_id_t = c_int;
pub const INVALID: c_int = 0;
pub const RUNNING: c_int = 1;
pub const RUNNABLE: c_int = 2;
pub const DYING: c_int = 3;
pub const DEAD: c_int = 4;
pub const STOPPED: c_int = 5;
pub const BLOCKED: c_int = 6;
pub const SLEEPING: c_int = 7;
pub const NO_CONTEXT: c_int = 8;
pub const enum_thread_state = c_uint;
pub const thread_state_t = enum_thread_state;
pub const struct_proc = opaque {};
pub const struct_tspckt = opaque {};
pub const struct_packet_payload = extern struct {
    m_data: ?*anyopaque,
    m_data_size: usize,
    m_code: driver_control_code_t,
    m_parent: ?*struct_tspckt,
};
pub const packet_payload_t = struct_packet_payload;
pub const SocketOnPacket = ?*const fn (packet_payload_t, [*c][*c]packet_response_t) callconv(.C) usize;
pub const struct_processor = opaque {};
pub const struct_spinlock = extern struct {
    m_lock: __spinlock_t,
    m_processor: ?*struct_processor,
    m_is_locked: [*c]atomic_ptr_t,
};
pub const spinlock_t = struct_spinlock;
pub const struct_queue_entry = extern struct {
    m_preceding_entry: [*c]struct_queue_entry,
    m_data: ?*anyopaque,
};
pub const queue_entry_t = struct_queue_entry;
pub const struct_queue = extern struct {
    m_head_ptr: [*c]queue_entry_t,
    m_tail_ptr: [*c]queue_entry_t,
    m_entries: usize,
    m_max_entries: usize,
};
pub const queue_t = struct_queue;
pub const struct_socket_packet_queue = extern struct {
    m_lock: [*c]spinlock_t,
    m_packets: [*c]queue_t,
};
pub const socket_packet_queue_t = struct_socket_packet_queue;
pub const THREADED_SOCKET_STATE_LISTENING: c_int = 0;
pub const THREADED_SOCKET_STATE_BUSY: c_int = 1;
pub const THREADED_SOCKET_STATE_BLOCKED: c_int = 2;
pub const enum_THREADED_SOCKET_STATE = c_uint;
pub const THREADED_SOCKET_STATE_t = enum_THREADED_SOCKET_STATE;
pub const struct_threaded_socket = extern struct {
    m_max_size_per_buffer: usize,
    m_port: u32,
    m_socket_flags: u32,
    f_on_packet: SocketOnPacket,
    f_exit_fn: FuncPtr,
    m_packet_queue: socket_packet_queue_t,
    m_state: THREADED_SOCKET_STATE_t,
    m_packet_mutex: [*c]struct_mutex,
    m_parent: [*c]struct_thread,
};
pub const struct_thread = extern struct {
    m_self: [*c]struct_thread,
    f_real_entry: ThreadEntry,
    f_entry_wrapper: ThreadEntryWrapper,
    f_exit: FuncPtr,
    m_lock: [*c]struct_mutex,
    m_mutex_list: [*c]list_t,
    m_context: thread_context_t,
    m_fpu_state: FpuState,
    m_has_been_scheduled: @"bool",
    m_name: [32]u8,
    m_tid: thread_id_t,
    m_cpu: u32,
    m_ticks_elapsed: u64,
    m_max_ticks: u64,
    m_kernel_stack_bottom: vaddr_t,
    m_kernel_stack_top: vaddr_t,
    m_user_stack_bottom: usize,
    m_user_stack_top: usize,
    m_current_state: thread_state_t,
    m_parent_proc: ?*struct_proc,
    f_relocated_entry_stub: ?*const fn (FuncPtr) callconv(.C) void,
    m_socket: [*c]struct_threaded_socket,
};
pub const thread_t = struct_thread;
pub const struct_mutex = extern struct {
    m_mutex_flags: u8,
    m_lock_depth: u32,
    m_lock_holder: [*c]thread_t,
    m_waiters: [*c]queue_t,
    m_lock: [*c]spinlock_t,
};
pub const mutex_t = struct_mutex;
pub const dev_url_t = [*c]const u8;
pub const struct_extern_driver = opaque {};
pub const struct_driver_ops = extern struct {
    f_write: ?*const fn ([*c]aniva_driver_t, ?*anyopaque, [*c]usize, usize) callconv(.C) c_int,
    f_read: ?*const fn ([*c]aniva_driver_t, ?*anyopaque, [*c]usize, usize) callconv(.C) c_int,
};
pub const driver_ops_t = struct_driver_ops;
pub const struct_drv_manifest = extern struct {
    m_handle: [*c]aniva_driver_t,
    m_dep_list: [*c]vector_t,
    m_dep_count: u32,
    m_dev_count: u32,
    m_flags: u32,
    m_check_version: driver_version_t,
    m_resources: [*c]kresource_bundle_t,
    m_lock: [*c]mutex_t,
    m_url: dev_url_t,
    m_url_length: usize,
    m_obj: ?*struct_oss_obj,
    m_driver_file_path: [*c]const u8,
    m_external: ?*struct_extern_driver,
    m_ops: driver_ops_t,
};
pub const DGROUP_TYPE_NONE: c_int = 0;
pub const DGROUP_TYPE_PCI: c_int = 1;
pub const DGROUP_TYPE_AHCI: c_int = 2;
pub const DGROUP_TYPE_IDE: c_int = 3;
pub const DGROUP_TYPE_ISA: c_int = 4;
pub const DGROUP_TYPE_USB: c_int = 5;
pub const DGROUP_TYPE_GPIO: c_int = 6;
pub const DGROUP_TYPE_I2C: c_int = 7;
pub const DGROUP_TYPE_VIDEO: c_int = 8;
pub const DGROUP_TYPE_MISC: c_int = 9;
pub const enum_DGROUP_TYPE = c_uint;
pub const struct_dgroup = extern struct {
    name: [*c]const u8,
    flags: u32,
    type: enum_DGROUP_TYPE,
    node: ?*struct_oss_node,
};
pub const ACPI_HANDLE = ?*anyopaque;
pub const acpi_handle_t = ACPI_HANDLE;
pub const struct_acpi_power_state = extern struct {
    resources: [*c]list_t,
};
pub const acpi_power_state_t = struct_acpi_power_state;
pub const struct_acpi_device_power = extern struct {
    c_state: c_int,
    states: [4]acpi_power_state_t,
};
pub const acpi_device_power_t = struct_acpi_device_power;
pub const struct_apci_device = extern struct {
    handle: acpi_handle_t,
    pwr: acpi_device_power_t,
    acpi_path: [*c]const u8,
    hid: [*c]const u8,
};
pub const acpi_device_t = struct_apci_device;
pub const ENDOINT_TYPE_INVALID: c_int = 0;
pub const ENDPOINT_TYPE_GENERIC: c_int = 1;
pub const ENDPOINT_TYPE_DISK: c_int = 2;
pub const ENDPOINT_TYPE_VIDEO: c_int = 3;
pub const ENDPOINT_TYPE_HID: c_int = 4;
pub const ENDPOINT_TYPE_PWM: c_int = 5;
pub const enum_ENDPOINT_TYPE = c_uint;
pub const dcc_t = driver_control_code_t;
pub const struct_device_generic_endpoint = extern struct {
    f_create: ?*const fn ([*c]struct_device) callconv(.C) c_int,
    f_msg: ?*const fn ([*c]struct_device, dcc_t) callconv(.C) usize,
    f_msg_ex: ?*const fn ([*c]struct_device, dcc_t, ?*anyopaque, usize, ?*anyopaque, usize) callconv(.C) usize,
};
pub const struct_device_disk_endpoint = extern struct {
    f_read: ?*const fn ([*c]struct_device, ?*anyopaque, usize, usize) callconv(.C) c_int,
    f_write: ?*const fn ([*c]struct_device, ?*anyopaque, usize, usize) callconv(.C) c_int,
    f_bread: ?*const fn ([*c]struct_device, ?*anyopaque, usize, usize) callconv(.C) c_int,
    f_bwrite: ?*const fn ([*c]struct_device, ?*anyopaque, usize, usize) callconv(.C) c_int,
    f_flush: ?*const fn ([*c]struct_device) callconv(.C) c_int,
};
pub const struct_device_video_endpoint = opaque {};
pub const struct_device_hid_endpoint = opaque {};
pub const struct_device_pwm_endpoint = extern struct {
    f_power_on: ?*const fn ([*c]struct_device) callconv(.C) c_int,
    f_remove: ?*const fn ([*c]struct_device) callconv(.C) c_int,
    f_suspend: ?*const fn ([*c]struct_device) callconv(.C) c_int,
    f_resume: ?*const fn ([*c]struct_device) callconv(.C) c_int,
};
const union_unnamed_2 = extern union {
    priv: ?*anyopaque,
    generic: [*c]struct_device_generic_endpoint,
    disk: [*c]struct_device_disk_endpoint,
    video: ?*struct_device_video_endpoint,
    hid: ?*struct_device_hid_endpoint,
    pwm: [*c]struct_device_pwm_endpoint,
};
pub const struct_device_endpoint = extern struct {
    type: enum_ENDPOINT_TYPE,
    size: u32,
    impl: union_unnamed_2,
};
pub const struct_device = extern struct {
    name: [*c]const u8,
    driver: [*c]struct_drv_manifest,
    obj: ?*struct_oss_obj,
    bus_group: [*c]struct_dgroup,
    private: ?*anyopaque,
    lock: [*c]mutex_t,
    acpi_dev: [*c]acpi_device_t,
    flags: u32,
    endpoint_count: u32,
    endpoints: [*c]struct_device_endpoint,
};
pub const DRV_DEPTYPE_URL: c_int = 0;
pub const DRV_DEPTYPE_PATH: c_int = 1;
pub const DRV_DEPTYPE_PROC: c_int = 2;
pub const enum_DRV_DEPTYPE = c_uint;
pub const struct_drv_dependency = extern struct {
    type: enum_DRV_DEPTYPE,
    flags: u32,
    location: [*c]const u8,
};
pub const struct_aniva_driver = extern struct {
    m_name: [128]u8,
    m_descriptor: [64]u8,
    m_version: driver_version_t,
    m_type: dev_type_t,
    m_precedence: drv_precedence_t,
    f_init: ?*const fn () callconv(.C) c_int,
    f_exit: ?*const fn () callconv(.C) c_int,
    f_msg: ?*const fn ([*c]struct_aniva_driver, driver_control_code_t, ?*anyopaque, usize, ?*anyopaque, usize) callconv(.C) usize,
    f_probe: ?*const fn ([*c]struct_aniva_driver, [*c]struct_device) callconv(.C) c_int,
    m_deps: [*c]struct_drv_dependency,
};
pub const STANDARD: c_int = 1;
pub const FROM_FILE: c_int = 2;
pub const LOADED_WITH_WARNING: c_int = 4;
pub const enum_dev_flags = c_uint;
pub const DEV_FLAGS = enum_dev_flags;
pub const struct_dev_constraint = extern struct {
    max_active: u32,
    current_active: u32,
    max_count: u32,
    current_count: u32,
    type: dev_type_t,
    res0: u8,
    res1: u16,
    res2: u32,
    active: [*c]struct_drv_manifest,
    core: [*c]struct_drv_manifest,
};
pub const dev_constraint_t = struct_dev_constraint;
pub extern var dev_type_urls: [9][*c]const u8;
pub extern fn init_aniva_driver_registry(...) void;
pub extern fn init_drivers(...) void;
pub extern fn allocate_dmanifest(...) [*c]struct_drv_manifest;
pub extern fn free_dmanifest(manifest: [*c]struct_drv_manifest) void;
pub extern fn install_driver(driver: [*c]struct_drv_manifest) ErrorOrPtr;
pub extern fn uninstall_driver(handle: [*c]struct_drv_manifest) ErrorOrPtr;
pub extern fn load_driver(manifest: [*c]struct_drv_manifest) ErrorOrPtr;
pub extern fn unload_driver(url: dev_url_t) ErrorOrPtr;
pub extern fn is_driver_installed(handle: [*c]struct_drv_manifest) @"bool";
pub extern fn is_driver_loaded(handle: [*c]struct_drv_manifest) @"bool";
pub extern fn get_driver(url: dev_url_t) [*c]struct_drv_manifest;
pub extern fn get_main_driver_from_type(@"type": dev_type_t) [*c]struct_drv_manifest;
pub extern fn get_driver_from_type(@"type": dev_type_t, index: u32) [*c]struct_drv_manifest;
pub extern fn get_core_driver(@"type": dev_type_t) [*c]struct_drv_manifest;
pub extern fn get_main_driver_path(buffer: [*c]u8, @"type": dev_type_t) c_int;
pub extern fn get_driver_type_count(@"type": dev_type_t) usize;
pub extern fn foreach_driver(callback: ?*const fn (?*struct_oss_node, ?*struct_oss_obj, ?*anyopaque) callconv(.C) @"bool", arg: ?*anyopaque) ErrorOrPtr;
pub extern fn set_main_driver(dev: [*c]struct_drv_manifest, @"type": dev_type_t) c_int;
pub extern fn verify_driver(manifest: [*c]struct_drv_manifest) @"bool";
pub extern fn replace_main_driver(manifest: [*c]struct_drv_manifest, uninstall: @"bool") void;
pub extern fn register_core_driver(driver: [*c]struct_aniva_driver, @"type": dev_type_t) c_int;
pub extern fn unregister_core_driver(driver: [*c]struct_aniva_driver) c_int;
pub extern fn try_driver_get(driver: [*c]struct_aniva_driver, flags: u32) [*c]struct_drv_manifest;
pub extern fn driver_set_ready(path: [*c]const u8) ErrorOrPtr;
pub extern fn driver_get_type_str(driver: [*c]struct_drv_manifest) [*c]const u8;
pub inline fn get_driver_type_url(arg_type: dev_type_t) [*c]const u8 {
    var @"type" = arg_type;
    return dev_type_urls[@"type"];
}
pub extern fn driver_send_msg(path: [*c]const u8, code: driver_control_code_t, buffer: ?*anyopaque, buffer_size: usize) ErrorOrPtr;
pub extern fn driver_send_msg_a(path: [*c]const u8, code: driver_control_code_t, buffer: ?*anyopaque, buffer_size: usize, resp_buffer: ?*anyopaque, resp_buffer_size: usize) ErrorOrPtr;
pub extern fn driver_send_msg_ex(driver: [*c]struct_drv_manifest, code: driver_control_code_t, buffer: ?*anyopaque, buffer_size: usize, resp_buffer: ?*anyopaque, resp_buffer_size: usize) ErrorOrPtr;
pub extern fn driver_send_msg_sync(path: [*c]const u8, code: driver_control_code_t, buffer: ?*anyopaque, buffer_size: usize) ErrorOrPtr;
pub extern fn driver_send_msg_sync_with_timeout(path: [*c]const u8, code: driver_control_code_t, buffer: ?*anyopaque, buffer_size: usize, mto: usize) ErrorOrPtr;
pub extern fn get_driver_url(handle: [*c]struct_aniva_driver) [*c]const u8;
pub extern fn get_driver_url_length(handle: [*c]struct_aniva_driver) usize;
pub const device_ep_t = struct_device_endpoint;
pub fn dev_is_valid_endpoint(arg_ep: [*c]device_ep_t) callconv(.C) @"bool" {
    var ep = arg_ep;
    return @as(@"bool", @intFromBool(((ep != null) and (ep.*.impl.priv != null)) and (ep.*.size != 0)));
}
pub const dgroup_t = struct_dgroup;
pub extern fn init_dgroups(...) void;
pub extern fn register_dev_group(@"type": enum_DGROUP_TYPE, name: [*c]const u8, flags: u32, parent: ?*struct_oss_node) [*c]dgroup_t;
pub extern fn unregister_dev_group(group: [*c]dgroup_t) c_int;
pub extern fn dev_group_get(name: [*c]const u8, out: [*c][*c]dgroup_t) c_int;
pub extern fn dev_group_getbus(group: [*c]dgroup_t, busnum: c_int) c_int;
pub extern fn dev_group_get_parent(group: [*c]dgroup_t) [*c]dgroup_t;
pub extern fn dev_group_get_device(group: [*c]dgroup_t, path: [*c]const u8, dev: [*c][*c]struct_device) c_int;
pub extern fn dev_group_add_device(group: [*c]dgroup_t, dev: [*c]struct_device) c_int;
pub extern fn dev_group_remove_device(group: [*c]dgroup_t, dev: [*c]struct_device) c_int;
pub extern fn create_queue(capacity: usize) [*c]queue_t;
pub extern fn create_limitless_queue(...) [*c]queue_t;
pub extern fn initialize_queue(queue: [*c]queue_t, capacity: usize) void;
pub extern fn destroy_queue(queue: [*c]queue_t, eliminate_entries: @"bool") ANIVA_STATUS;
pub extern fn queue_enqueue(queue: [*c]queue_t, data: ?*anyopaque) void;
pub extern fn queue_dequeue(queue: [*c]queue_t) ?*anyopaque;
pub extern fn queue_peek(queue: [*c]queue_t) ?*anyopaque;
pub extern fn queue_ensure_capacity(queue: [*c]queue_t, capacity: usize) ANIVA_STATUS;
pub const union_pml_entry = extern union {
    raw_bits: u64 align(1),
};
pub const pml_entry_t = union_pml_entry;
pub inline fn pml_entry_is_bit_set(arg_entry: [*c]pml_entry_t, arg_bit: usize) @"bool" {
    var entry = arg_entry;
    var bit = arg_bit;
    return @as(@"bool", @intFromBool((entry.*.raw_bits & bit) == bit));
}
pub inline fn pml_entry_set_bit(arg_entry: [*c]pml_entry_t, arg_bit: usize, arg_value: @"bool") void {
    var entry = arg_entry;
    var bit = arg_bit;
    var value = arg_value;
    if (value != 0) {
        entry.*.raw_bits |= bit;
    } else {
        entry.*.raw_bits &= ~bit;
    }
}
pub const gdt_pointer_t = extern struct {
    limit: u16 align(1),
    base: usize align(1),
};
pub const struct_tss_entry = extern struct {
    reserved_0: u32 align(1),
    rsp0l: u32 align(1),
    rsp0h: u32 align(1),
    rsp1l: u32 align(1),
    rsp1h: u32 align(1),
    rsp2l: u32 align(1),
    rsp2h: u32 align(1),
    reserved_1: u64 align(1),
    ist: [7]u64 align(1),
    reserved_2: u64 align(1),
    reserved_3: u16 align(1),
    iomap_base: u16 align(1),
};
pub const tss_entry_t = struct_tss_entry; // ./../src/aniva/system/processor/gdt.h:45:13: warning: struct demoted to opaque type - has bitfield
const struct_unnamed_3 = opaque {};
const struct_unnamed_4 = extern struct {
    low: u32,
    high: u32,
};
pub const gdt_entry_t = extern union {
    structured: struct_unnamed_3 align(1),
    unnamed_0: struct_unnamed_4 align(1),
    raw: usize align(1),
};
pub inline fn get_gdte_base(arg_entry: ?*gdt_entry_t) usize {
    var entry = arg_entry;
    var base: usize = @as(usize, @bitCast(@as(c_ulonglong, entry.*.structured.base_low)));
    base |= @as(usize, @bitCast(@as(c_longlong, @as(c_int, @bitCast(@as(c_uint, entry.*.structured.base_high))) << @intCast(16))));
    base |= @as(usize, @bitCast(@as(c_longlong, @as(c_int, @bitCast(@as(c_uint, entry.*.structured.base_hi2))) << @intCast(24))));
    return base;
}
pub inline fn set_gdte_base(arg_entry: ?*gdt_entry_t, arg_base: usize) void {
    var entry = arg_entry;
    var base = arg_base;
    entry.*.structured.base_low = @as(u16, @bitCast(@as(c_ushort, @truncate(base & @as(usize, @bitCast(@as(c_ulonglong, @as(c_uint, 65535))))))));
    entry.*.structured.base_high = @as(u8, @bitCast(@as(u8, @truncate((base >> @intCast(16)) & @as(usize, @bitCast(@as(c_ulonglong, @as(c_uint, 255))))))));
    entry.*.structured.base_hi2 = @as(u8, @bitCast(@as(u8, @truncate((base >> @intCast(24)) & @as(usize, @bitCast(@as(c_ulonglong, @as(c_uint, 255))))))));
}
pub inline fn set_gdte_limit(arg_entry: ?*gdt_entry_t, arg_limit: u32) void {
    var entry = arg_entry;
    var limit = arg_limit;
    entry.*.structured.limit_low = @as(u16, @bitCast(@as(c_ushort, @truncate(limit & @as(u32, @bitCast(@as(c_int, 65535)))))));
    entry.*.structured.limit_high = @as(u8, @bitCast(@as(u8, @truncate((limit >> @intCast(16)) & @as(u32, @bitCast(@as(c_int, 15)))))));
}
pub const thread_context_t = extern struct {
    rdi: usize,
    rsi: usize,
    rbp: usize,
    rdx: usize,
    rcx: usize,
    rbx: usize,
    rax: usize,
    r8: usize,
    r9: usize,
    r10: usize,
    r11: usize,
    r12: usize,
    r13: usize,
    r14: usize,
    r15: usize,
    rflags: usize,
    rip: usize,
    rsp: usize,
    rsp0: usize,
    cs: usize,
    cr3: usize,
};
pub inline fn setup_regs(arg_kernel: @"bool", arg_root_table: [*c]pml_entry_t, arg_stack_top: usize) thread_context_t {
    var kernel = arg_kernel;
    var root_table = arg_root_table;
    var stack_top = arg_stack_top;
    var regs: thread_context_t = thread_context_t{
        .rdi = @as(usize, @bitCast(@as(c_longlong, @as(c_int, 0)))),
        .rsi = @import("std").mem.zeroes(usize),
        .rbp = @import("std").mem.zeroes(usize),
        .rdx = @import("std").mem.zeroes(usize),
        .rcx = @import("std").mem.zeroes(usize),
        .rbx = @import("std").mem.zeroes(usize),
        .rax = @import("std").mem.zeroes(usize),
        .r8 = @import("std").mem.zeroes(usize),
        .r9 = @import("std").mem.zeroes(usize),
        .r10 = @import("std").mem.zeroes(usize),
        .r11 = @import("std").mem.zeroes(usize),
        .r12 = @import("std").mem.zeroes(usize),
        .r13 = @import("std").mem.zeroes(usize),
        .r14 = @import("std").mem.zeroes(usize),
        .r15 = @import("std").mem.zeroes(usize),
        .rflags = @import("std").mem.zeroes(usize),
        .rip = @import("std").mem.zeroes(usize),
        .rsp = @import("std").mem.zeroes(usize),
        .rsp0 = @import("std").mem.zeroes(usize),
        .cs = @import("std").mem.zeroes(usize),
        .cr3 = @import("std").mem.zeroes(usize),
    };
    regs.cs = @as(usize, @bitCast(@as(c_longlong, @as(c_int, 32) | @as(c_int, 3))));
    regs.rflags = @as(usize, @bitCast(@as(c_longlong, @as(c_int, 514))));
    regs.rsp0 = stack_top;
    regs.cr3 = @as(usize, @intCast(@intFromPtr(root_table)));
    if (kernel != 0) {
        regs.cs = 8;
        regs.rsp = stack_top;
    }
    return regs;
}
pub inline fn contex_set_rip(arg_ctx: [*c]thread_context_t, arg_rip: usize, arg_arg0: usize, arg_arg1: usize) void {
    var ctx = arg_ctx;
    var rip = arg_rip;
    var arg0 = arg_arg0;
    var arg1 = arg_arg1;
    ctx.*.rip = rip;
    ctx.*.rdi = arg0;
    ctx.*.rsi = arg1;
}
pub extern fn init_packet_payloads(...) void;
pub extern fn init_packet_payload(payload: [*c]packet_payload_t, parent: ?*struct_tspckt, data: ?*anyopaque, size: usize, code: driver_control_code_t) void;
pub extern fn destroy_packet_payload(payload: [*c]packet_payload_t) void;
pub const memory_order_relaxed: c_int = 0;
pub const memory_order_consume: c_int = 1;
pub const memory_order_acquire: c_int = 2;
pub const memory_order_release: c_int = 3;
pub const memory_order_acq_rel: c_int = 4;
pub const memory_order_seq_cst: c_int = 5;
pub const MemOrder = c_uint; // ./../src/aniva/libk/atomic.h:18:3: warning: TODO implement translation of stmt class AtomicExprClass
// ./../src/aniva/libk/atomic.h:17:27: warning: unable to translate function, demoted to extern
pub extern fn _atomic_store(arg_var: [*c]?*volatile anyopaque, arg_d: ?*anyopaque, arg_order: MemOrder) void; // ./../src/aniva/libk/atomic.h:22:3: warning: TODO implement translation of stmt class AtomicExprClass
// ./../src/aniva/libk/atomic.h:21:27: warning: unable to translate function, demoted to extern
pub extern fn _atomic_store_alt(arg_var: [*c]volatile usize, arg_d: usize, arg_order: MemOrder) void; // ./../src/aniva/libk/atomic.h:26:10: warning: TODO implement translation of stmt class AtomicExprClass
// ./../src/aniva/libk/atomic.h:25:28: warning: unable to translate function, demoted to extern
pub extern fn _atomic_load(arg_var: [*c]?*volatile anyopaque, arg_order: MemOrder) ?*anyopaque; // ./../src/aniva/libk/atomic.h:30:10: warning: TODO implement translation of stmt class AtomicExprClass
// ./../src/aniva/libk/atomic.h:29:32: warning: unable to translate function, demoted to extern
pub extern fn _atomic_load_alt(arg_var: [*c]volatile usize, arg_order: MemOrder) usize; // ./../src/aniva/libk/atomic.h:38:3: warning: TODO implement function '__atomic_signal_fence' in std.zig.c_builtins
// ./../src/aniva/libk/atomic.h:36:27: warning: unable to translate function, demoted to extern
pub extern fn full_mem_barier() void;
pub inline fn atomicStore(arg_var: [*c]?*volatile anyopaque, arg_d: ?*anyopaque) void {
    var @"var" = arg_var;
    var d = arg_d;
    _atomic_store(@"var", d, @as(c_uint, @bitCast(memory_order_seq_cst)));
}
pub inline fn atomicStore_alt(arg_var: [*c]volatile usize, arg_d: usize) void {
    var @"var" = arg_var;
    var d = arg_d;
    _atomic_store_alt(@"var", d, @as(c_uint, @bitCast(memory_order_seq_cst)));
}
pub inline fn atomicLoad(arg_var: [*c]?*volatile anyopaque) ?*anyopaque {
    var @"var" = arg_var;
    return _atomic_load(@"var", @as(c_uint, @bitCast(memory_order_seq_cst)));
}
pub inline fn atomicLoad_alt(arg_var: [*c]volatile usize) usize {
    var @"var" = arg_var;
    return _atomic_load_alt(@"var", @as(c_uint, @bitCast(memory_order_seq_cst)));
} // ./../src/aniva/sync/atomic_ptr.h:8:30: warning: unsupported type: 'Atomic'
// ./../src/aniva/sync/atomic_ptr.h:7:9: warning: struct demoted to opaque type - unable to translate type of field __val
pub const atomic_ptr_t = opaque {};
pub extern fn init_atomic_ptr(ptr: ?*atomic_ptr_t, value: usize) c_int;
pub extern fn create_atomic_ptr(...) ?*atomic_ptr_t;
pub extern fn create_atomic_ptr_ex(initial_value: usize) ?*atomic_ptr_t;
pub extern fn destroy_atomic_ptr(ptr: ?*atomic_ptr_t) void;
pub extern fn atomic_ptr_read(ptr: ?*atomic_ptr_t) usize;
pub extern fn atomic_ptr_write(ptr: ?*atomic_ptr_t, value: usize) void;
pub const __spinlock_t = extern struct {
    m_latch: [1]c_int,
    m_cpu_num: c_int,
    m_func: [*c]const u8,
};
pub extern fn create_spinlock(...) [*c]spinlock_t;
pub extern fn init_spinlock(lock: [*c]spinlock_t) void;
pub extern fn destroy_spinlock(lock: [*c]spinlock_t) void;
pub extern fn spinlock_lock(lock: [*c]spinlock_t) void;
pub extern fn spinlock_unlock(lock: [*c]spinlock_t) void;
pub extern fn spinlock_is_locked(lock: [*c]spinlock_t) @"bool";
pub const struct_registers = extern struct {
    rdi: usize align(1),
    rsi: usize align(1),
    rbp: usize align(1),
    rdx: usize align(1),
    rcx: usize align(1),
    rbx: usize align(1),
    rax: usize align(1),
    r8: usize align(1),
    r9: usize align(1),
    r10: usize align(1),
    r11: usize align(1),
    r12: usize align(1),
    r13: usize align(1),
    r14: usize align(1),
    r15: usize align(1),
    isr_no: usize align(1),
    err_code: usize align(1),
    rip: usize align(1),
    cs: usize align(1),
    rflags: usize align(1),
    us_rsp: usize align(1),
    ss: usize align(1),
};
pub const registers_t = struct_registers;
pub const TS_REGISTERED: c_int = 1;
pub const TS_ACTIVE: c_int = 2;
pub const TS_BUSY: c_int = 4;
pub const TS_IS_CLOSED: c_int = 8;
pub const TS_SHOULD_EXIT: c_int = 16;
pub const TS_READY: c_int = 32;
pub const enum_THREADED_SOCKET_FLAGS = c_uint;
pub const THREADED_SOCKET_FLAGS_t = enum_THREADED_SOCKET_FLAGS;
pub const threaded_socket_t = struct_threaded_socket;
pub extern fn init_socket(...) void;
pub extern fn create_threaded_socket(parent: [*c]struct_thread, exit_fn: FuncPtr, on_packet_fn: SocketOnPacket, port: u32, max_size_per_buffer: usize) [*c]threaded_socket_t;
pub extern fn destroy_threaded_socket(ptr: [*c]threaded_socket_t) ANIVA_STATUS;
pub extern fn socket_enable(socket: [*c]struct_thread) ErrorOrPtr;
pub extern fn socket_disable(socket: [*c]struct_thread) ErrorOrPtr;
pub extern fn socket_register_pckt_func(@"fn": SocketOnPacket) ErrorOrPtr;
pub extern fn socket_set_flag(ptr: [*c]threaded_socket_t, flag: THREADED_SOCKET_FLAGS_t, value: @"bool") void;
pub extern fn socket_is_flag_set(ptr: [*c]threaded_socket_t, flag: THREADED_SOCKET_FLAGS_t) @"bool";
pub extern fn socket_try_handle_tspacket(socket: [*c]threaded_socket_t) ErrorOrPtr;
pub extern fn socket_handle_tspacket(packet: ?*struct_tspckt) ErrorOrPtr;
pub extern fn socket_try_verifiy_port(socket: [*c]threaded_socket_t) ErrorOrPtr;
pub extern fn socket_verify_port(socket: [*c]threaded_socket_t) u32;
pub const X87: c_int = 1;
pub const SSE: c_int = 2;
pub const AVX: c_int = 4;
pub const MPX_BNDREGS: c_int = 8;
pub const MPX_BNDCSR: c_int = 16;
pub const AVX512_opmask: c_int = 32;
pub const AVX512_ZMM_hi: c_int = 64;
pub const AVX512_ZMM: c_int = 128;
pub const PT: c_int = 256;
pub const PKRU: c_int = 512;
pub const CET_U: c_int = 2048;
pub const CET_S: c_int = 4096;
pub const HDC: c_int = 8192;
pub const LBR: c_int = 32768;
pub const HWP: c_int = 65536;
pub const XCOMP_ENABLE: c_ulong = 9223372036854775808;
pub const enum_state_comp = c_ulong;
pub const state_comp_t = enum_state_comp; // ./../src/aniva/system/processor/fpu/state.h:29:11: warning: struct demoted to opaque type - has bitfield
pub const legacy_region_t = opaque {};
pub const state_header_t = extern struct {
    xstate: state_comp_t align(1),
    xcomp: state_comp_t align(1),
    rsrv: [48]u8 align(1),
};
pub const FpuState = extern struct {
    region: legacy_region_t align(1),
    header: state_header_t align(1),
    esa: [256]u8 align(1),
};
pub extern fn save_fpu_state(buffer: ?*FpuState) void;
pub extern fn store_fpu_state(buffer: ?*FpuState) void;
pub extern fn create_vector(capacity: usize, entry_size: u16, flags: u32) [*c]vector_t;
pub extern fn init_vector(vec: [*c]vector_t, capacity: usize, entry_size: u16, flags: u32) void;
pub extern fn destroy_vector(ptr: [*c]vector_t) void;
pub extern fn vector_get([*c]vector_t, u32) ?*anyopaque;
pub extern fn vector_indexof([*c]vector_t, ?*anyopaque) ErrorOrPtr;
pub extern fn vector_add([*c]vector_t, ?*anyopaque) ErrorOrPtr;
pub extern fn vector_remove([*c]vector_t, u32) ErrorOrPtr;
pub const thread_entry_stub: [*c]u8 = @extern([*c]u8, .{
    .name = "thread_entry_stub",
});
pub const thread_entry_stub_end: [*c]u8 = @extern([*c]u8, .{
    .name = "thread_entry_stub_end",
});
pub extern fn init_proc_core(...) ANIVA_STATUS;
pub extern fn socket_register(socket: [*c]struct_threaded_socket) ErrorOrPtr;
pub extern fn socket_unregister(socket: [*c]struct_threaded_socket) ErrorOrPtr;
pub const proc_id_t = c_int;
pub const full_proc_id_t = u64;
pub const fid_t = full_proc_id_t;
const struct_unnamed_5 = extern struct {
    thread_id: thread_id_t,
    proc_id: proc_id_t,
};
pub const u_full_proc_id_t = extern union {
    unnamed_0: struct_unnamed_5,
    id: fid_t,
};
pub const u_fid_t = u_full_proc_id_t;
pub fn create_full_procid(arg_proc_id: u32, arg_thread_id: u32) callconv(.C) full_proc_id_t {
    var proc_id = arg_proc_id;
    var thread_id = arg_thread_id;
    var ret: u_full_proc_id_t = undefined;
    ret.unnamed_0.proc_id = @as(proc_id_t, @bitCast(proc_id));
    ret.unnamed_0.thread_id = @as(thread_id_t, @bitCast(thread_id));
    return ret.id;
}
pub fn full_procid_get_tid(arg_fprocid: full_proc_id_t) callconv(.C) u32 {
    var fprocid = arg_fprocid;
    var u_id: u_full_proc_id_t = undefined;
    u_id.id = fprocid;
    return @as(u32, @bitCast(u_id.unnamed_0.thread_id));
}
pub fn full_procid_get_pid(arg_fprocid: full_proc_id_t) callconv(.C) proc_id_t {
    var fprocid = arg_fprocid;
    var u_id: u_full_proc_id_t = undefined;
    u_id.id = fprocid;
    return u_id.unnamed_0.proc_id;
}
pub extern fn generate_new_proc_id(...) ErrorOrPtr;
pub extern fn get_registered_sockets(...) list_t;
pub extern fn spawn_thread(name: [*c]u8, entry: FuncPtr, arg0: u64) ?*struct_thread;
pub extern fn find_registered_socket(port: u32) [*c]struct_threaded_socket;
pub extern fn find_proc_by_id(id: proc_id_t) ?*struct_proc;
pub extern fn find_proc(name: [*c]const u8) ?*struct_proc;
pub extern fn foreach_proc(f_callback: ?*const fn (?*struct_proc) callconv(.C) @"bool") @"bool";
pub extern fn get_proc_count(...) u32;
pub extern fn find_thread_by_fid(fid: full_proc_id_t) ?*struct_thread;
pub extern fn find_thread(proc: ?*struct_proc, tid: thread_id_t) ?*struct_thread;
pub extern fn proc_register(proc: ?*struct_proc) ErrorOrPtr;
pub extern fn proc_unregister(name: [*c]u8) ErrorOrPtr;
pub extern fn current_proc_is_kernel(...) @"bool";
pub extern fn send_packet_to_socket(port: u32, buffer: ?*anyopaque, buffer_size: usize) ErrorOrPtr;
pub extern fn send_packet_to_socket_ex(port: u32, code: driver_control_code_t, buffer: ?*anyopaque, buffer_size: usize) ErrorOrPtr;
pub extern fn validate_tspckt(packet: ?*struct_tspckt) @"bool";
pub extern fn generate_tspckt_identifier(packet: ?*struct_tspckt) ErrorOrPtr;
pub extern fn create_thread(FuncPtr, ThreadEntryWrapper, usize, [*c]const u8, ?*struct_proc, @"bool") ?*thread_t;
pub extern fn create_thread_for_proc(?*struct_proc, FuncPtr, usize, [*c]const u8) ?*thread_t;
pub extern fn create_thread_as_socket(process: ?*struct_proc, entry: FuncPtr, arg0: usize, exit_fn: FuncPtr, on_packet_fn: SocketOnPacket, name: [*c]u8, port: [*c]u32) ?*thread_t;
pub extern fn bootstrap_thread_entries(thread: ?*thread_t) void;
pub extern fn thread_switch_context(from: ?*thread_t, to: ?*thread_t) void;
pub extern fn thread_prepare_context(?*thread_t) ANIVA_STATUS;
pub extern fn thread_set_state(?*thread_t, thread_state_t) void;
pub extern fn destroy_thread(?*thread_t) ANIVA_STATUS;
pub extern fn get_generic_idle_thread(...) ?*thread_t;
pub inline fn thread_is_socket(arg_t: ?*thread_t) @"bool" {
    var t = arg_t;
    return @as(@"bool", @intFromBool(t.*.m_socket != @as([*c]struct_threaded_socket, @ptrCast(@alignCast(@as(?*anyopaque, @ptrFromInt(@as(c_int, 0))))))));
}
pub inline fn thread_has_been_scheduled_before(arg_t: ?*thread_t) @"bool" {
    var t = arg_t;
    return t.*.m_has_been_scheduled;
}
pub extern fn thread_try_revert_userpacket_context(regs: [*c]registers_t, thread: ?*thread_t) @"bool";
pub extern fn thread_try_prepare_userpacket(to: ?*thread_t) void;
pub extern fn thread_register_mutex(thread: ?*thread_t, lock: [*c]struct_mutex) void;
pub extern fn thread_unregister_mutex(thread: ?*thread_t, lock: [*c]struct_mutex) void;
pub extern fn thread_block(thread: ?*thread_t) void;
pub extern fn thread_unblock(thread: ?*thread_t) void;
pub extern fn thread_sleep(thread: ?*thread_t) void;
pub extern fn thread_wakeup(thread: ?*thread_t) void;
pub extern fn create_mutex(flags: u8) [*c]mutex_t;
pub extern fn init_mutex(lock: [*c]mutex_t, flags: u8) void;
pub extern fn clear_mutex(mutex: [*c]mutex_t) void;
pub extern fn destroy_mutex(mutex: [*c]mutex_t) void;
pub extern fn mutex_lock(mutex: [*c]mutex_t) void;
pub extern fn mutex_unlock(mutex: [*c]mutex_t) void;
pub extern fn mutex_is_locked(mutex: [*c]mutex_t) @"bool";
pub extern fn mutex_is_locked_by_current_thread(mutex: [*c]mutex_t) @"bool";
pub const struct_bitmap = extern struct {
    m_default: u8 align(8),
    m_size: usize,
    m_entries: usize,
    pub fn m_map(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 24)));
    }
};
pub const bitmap_t = struct_bitmap;
pub const FORWARDS: c_int = 0;
pub const BACKWARDS: c_int = 1;
pub const enum_BITMAP_SEARCH_DIR = c_uint;
pub extern fn create_bitmap(size: usize) [*c]bitmap_t;
pub extern fn create_bitmap_with_default(size: usize, default_value: u8) [*c]bitmap_t;
pub extern fn create_bitmap_ex(max_entries: usize, default_value: u8) [*c]bitmap_t;
pub extern fn init_bitmap(bitmap: [*c]bitmap_t, max_entries: usize, default_value: u8) void;
pub extern fn destroy_bitmap(map: [*c]bitmap_t) void;
pub extern fn bitmap_mark(this: [*c]bitmap_t, index: u32) void;
pub extern fn bitmap_unmark(this: [*c]bitmap_t, index: u32) void;
pub extern fn bitmap_isset(this: [*c]bitmap_t, index: u32) @"bool";
pub extern fn bitmap_find_free(this: [*c]bitmap_t) ErrorOrPtr;
pub extern fn bitmap_find_free_range(this: [*c]bitmap_t, length: usize) ErrorOrPtr;
pub extern fn bitmap_find_free_range_from(this: [*c]bitmap_t, length: usize, start_idx: usize) ErrorOrPtr;
pub extern fn bitmap_find_free_ex(this: [*c]bitmap_t, dir: enum_BITMAP_SEARCH_DIR) ErrorOrPtr;
pub extern fn bitmap_find_free_range_ex(this: [*c]bitmap_t, length: usize, dir: enum_BITMAP_SEARCH_DIR) ErrorOrPtr;
pub extern fn bitmap_find_free_range_from_ex(this: [*c]bitmap_t, length: usize, start_idx: usize, dir: enum_BITMAP_SEARCH_DIR) ErrorOrPtr;
pub extern fn bitmap_mark_range(this: [*c]bitmap_t, index: u32, length: usize) void;
pub extern fn bitmap_unmark_range(this: [*c]bitmap_t, index: u32, length: usize) void;
pub const ZONE_USED: c_int = 1;
pub const ZONE_PERMANENT: c_int = 2;
pub const ZONE_SHOULD_ZERO: c_int = 4;
pub const enum_ZONE_FLAGS = c_uint;
pub const ZONE_FLAGS_t = enum_ZONE_FLAGS;
pub const ZALLOC_8BYTES: c_int = 8;
pub const ZALLOC_16BYTES: c_int = 16;
pub const ZALLOC_32BYTES: c_int = 32;
pub const ZALLOC_64BYTES: c_int = 64;
pub const ZALLOC_128BYTES: c_int = 128;
pub const ZALLOC_256BYTES: c_int = 256;
pub const ZALLOC_512BYTES: c_int = 512;
pub const ZALLOC_1024BYTES: c_int = 1024;
pub const enum_ZONE_ENTRY_SIZE = c_uint;
pub const ZONE_ENTRY_SIZE_t = enum_ZONE_ENTRY_SIZE;
pub const struct_zone = extern struct {
    m_zone_entry_size: usize,
    m_total_available_size: usize,
    m_entries_start: vaddr_t,
    m_entries: bitmap_t,
};
pub const zone_t = struct_zone;
pub const struct_zone_store = extern struct {
    m_zones_count: usize align(1),
    m_capacity: usize align(1),
    m_next: [*c]struct_zone_store align(1),
    pub fn m_zones(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), [*c]zone_t) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), [*c]zone_t);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 24)));
    }
};
pub const zone_store_t = struct_zone_store;
pub const struct_zone_allocator = extern struct {
    m_store: [*c]zone_store_t,
    m_lock: [*c]mutex_t,
    m_store_count: usize,
    m_grow_size: usize,
    m_total_size: usize,
    m_flags: u32,
    m_res0: u32,
    m_entry_size: enum_ZONE_ENTRY_SIZE,
    m_next: [*c]struct_zone_allocator,
};
pub const zone_allocator_t = struct_zone_allocator;
pub extern fn init_zalloc(...) void;
pub extern fn kzalloc(size: usize) ?*anyopaque;
pub extern fn kzfree(address: ?*anyopaque, size: usize) void;
pub extern fn zalloc(allocator: [*c]zone_allocator_t, size: usize) ?*anyopaque;
pub extern fn zfree(allocator: [*c]zone_allocator_t, address: ?*anyopaque, size: usize) void;
pub extern fn zalloc_fixed(allocator: [*c]zone_allocator_t) ?*anyopaque;
pub extern fn zfree_fixed(allocator: [*c]zone_allocator_t, address: ?*anyopaque) void;
pub extern fn create_zone_allocator(initial_size: usize, hard_max_entry_size: usize, flags: usize) [*c]zone_allocator_t;
pub extern fn create_zone_allocator_at(start_addr: vaddr_t, initial_size: usize, flags: usize) [*c]zone_allocator_t;
pub extern fn create_zone_allocator_ex(map: [*c]pml_entry_t, start_addr: vaddr_t, initial_size: usize, hard_max_entry_size: usize, flags: usize) [*c]zone_allocator_t;
pub extern fn init_zone_allocator(allocator: [*c]zone_allocator_t, initial_size: usize, hard_max_entry_size: usize, flags: usize) ErrorOrPtr;
pub extern fn init_zone_allocator_ex(allocator: [*c]zone_allocator_t, map: [*c]pml_entry_t, start_addr: vaddr_t, initial_size: usize, hard_max_entry_size: usize, flags: usize) ErrorOrPtr;
pub extern fn destroy_zone_allocator(allocator: [*c]zone_allocator_t, clear_zones: @"bool") void;
pub extern fn zone_allocator_clear(allocator: [*c]zone_allocator_t) void;
pub extern fn create_zone_store(initial_capacity: usize) [*c]zone_store_t;
pub extern fn destroy_zone_store(allocator: [*c]zone_allocator_t, store: [*c]zone_store_t) void;
pub extern fn destroy_zone_stores(allocator: [*c]zone_allocator_t) void;
pub extern fn allocator_add_zone(allocator: [*c]zone_allocator_t, zone: [*c]zone_t) ErrorOrPtr;
pub extern fn allocator_remove_zone(allocator: [*c]zone_allocator_t, zone: [*c]zone_t) ErrorOrPtr;
pub extern fn zone_store_add(store: [*c]zone_store_t, zone: [*c]zone_t) ErrorOrPtr;
pub extern fn zone_store_remove(store: [*c]zone_store_t, zone: [*c]zone_t) ErrorOrPtr;
pub extern fn create_zone(entry_size: usize, max_entries: usize) [*c]zone_t;
pub extern fn destroy_zone(allocator: [*c]zone_allocator_t, zone: [*c]zone_t) void;
pub extern fn isalnum(c: c_int) c_int;
pub extern fn isalpha(c: c_int) c_int;
pub extern fn isdigit(c: c_int) c_int;
pub extern fn islower(c: c_int) c_int;
pub extern fn isprint(c: c_int) c_int;
pub extern fn iscntrl(c: c_int) c_int;
pub extern fn isgraph(c: c_int) c_int;
pub extern fn ispunct(c: c_int) c_int;
pub extern fn isspace(c: c_int) c_int;
pub extern fn isupper(c: c_int) c_int;
pub extern fn isxdigit(c: c_int) c_int;
pub extern fn isascii(c: c_int) c_int;
pub extern fn tolower(c: c_int) c_int;
pub extern fn toupper(c: c_int) c_int;
pub extern fn strlen(str: [*c]const u8) usize;
pub extern fn strcmp(str1: [*c]const u8, str2: [*c]const u8) c_int;
pub extern fn strncmp(s1: [*c]const u8, s2: [*c]const u8, n: usize) c_int;
pub extern fn strcpy(dest: [*c]u8, src: [*c]const u8) [*c]u8;
pub extern fn strncpy(dest: [*c]u8, src: [*c]const u8, len: usize) [*c]u8;
pub extern fn strdup(str: [*c]const u8) [*c]u8;
pub extern fn strcat([*c]u8, [*c]const u8) [*c]u8;
pub extern fn strncat([*c]u8, [*c]const u8, usize) [*c]u8;
pub extern fn strstr(h: [*c]const u8, n: [*c]const u8) [*c]u8;
pub extern fn memcmp(dest: ?*const anyopaque, src: ?*const anyopaque, size: usize) @"bool";
pub extern fn memcpy(noalias dest: ?*anyopaque, noalias src: ?*const anyopaque, length: usize) ?*anyopaque;
pub extern fn memmove(dest: ?*anyopaque, src: ?*const anyopaque, n: usize) ?*anyopaque;
pub extern fn memset(data: ?*anyopaque, value: c_int, length: usize) ?*anyopaque;
pub extern fn memchr(s: ?*const anyopaque, c: c_int, n: usize) ?*anyopaque;
pub extern fn concat(one: [*c]u8, two: [*c]u8, out: [*c]u8) c_int;
pub extern fn to_string(val: u64) [*c]const u8;
pub extern fn dirty_strtoul(cp: [*c]const u8, endp: [*c][*c]u8, base: c_uint) u64;
pub extern fn read_cr0(...) usize;
pub extern fn read_cr2(...) usize;
pub extern fn read_cr3(...) usize;
pub extern fn read_cr4(...) usize;
pub extern fn read_xcr0(...) u64;
pub extern fn write_cr0(value: usize) void;
pub extern fn write_cr2(value: usize) void;
pub extern fn write_cr3(value: usize) void;
pub extern fn write_cr4(value: usize) void;
pub extern fn write_xcr0(value: u64) void;
pub extern fn read_cpuid(eax: u32, ecx: u32, eax_out: [*c]u32, ebx_out: [*c]u32, ecx_out: [*c]u32, edx_out: [*c]u32) void; // ./../src/aniva/libk/stddef.h:40:13: warning: TODO implement translation of stmt class GCCAsmStmtClass
// ./../src/aniva/system/asm_specifics.h:23:25: warning: unable to translate function, demoted to extern
pub extern fn get_eflags() usize; // ./../src/aniva/libk/stddef.h:40:13: warning: TODO implement translation of stmt class GCCAsmStmtClass
// ./../src/aniva/system/asm_specifics.h:36:20: warning: unable to translate function, demoted to extern
pub extern fn write_gs(arg_offset: u32, arg_val: usize) void; // ./../src/aniva/libk/stddef.h:40:13: warning: TODO implement translation of stmt class GCCAsmStmtClass
// ./../src/aniva/system/asm_specifics.h:40:25: warning: unable to translate function, demoted to extern
pub extern fn read_gs(arg_offset: u32) usize; // ./../src/aniva/libk/stddef.h:40:13: warning: TODO implement translation of stmt class GCCAsmStmtClass
// ./../src/aniva/system/asm_specifics.h:46:20: warning: unable to translate function, demoted to extern
pub extern fn write_fs(arg_offset: u32, arg_val: usize) void; // ./../src/aniva/libk/stddef.h:40:13: warning: TODO implement translation of stmt class GCCAsmStmtClass
// ./../src/aniva/system/asm_specifics.h:50:25: warning: unable to translate function, demoted to extern
pub extern fn read_fs(arg_offset: u32) usize; // ./../src/aniva/libk/stddef.h:40:13: warning: TODO implement translation of stmt class GCCAsmStmtClass
// ./../src/aniva/system/asm_specifics.h:56:25: warning: unable to translate function, demoted to extern
pub extern fn read_cs() usize; // ./../src/aniva/libk/stddef.h:40:13: warning: TODO implement translation of stmt class GCCAsmStmtClass
// ./../src/aniva/system/asm_specifics.h:62:20: warning: unable to translate function, demoted to extern
pub extern fn __ltr(arg_sel: u16) void;
pub const BOOLEAN = u8;
pub const UINT8 = u8;
pub const UINT16 = c_ushort;
pub const INT16 = c_short;
pub const UINT64 = c_ulonglong;
pub const INT64 = c_longlong;
pub const UINT32 = c_uint;
pub const INT32 = c_int;
pub const ACPI_NATIVE_INT = INT64;
pub const ACPI_SIZE = UINT64;
pub const ACPI_IO_ADDRESS = UINT64;
pub const ACPI_PHYSICAL_ADDRESS = UINT64;
pub const ACPI_STATUS = UINT32;
pub const ACPI_NAME = UINT32;
pub const ACPI_STRING = [*c]u8;
pub const ACPI_OWNER_ID = UINT16;
pub const ACPI_INTEGER = UINT64;
pub const ACPI_OBJECT_TYPE = UINT32;
pub const ACPI_EVENT_TYPE = UINT32;
pub const ACPI_EVENT_STATUS = UINT32;
pub const ACPI_ADR_SPACE_TYPE = UINT8;
pub const ACPI_SLEEP_FUNCTION = ?*const fn (UINT8) callconv(.C) ACPI_STATUS;
pub const struct_acpi_sleep_functions = extern struct {
    LegacyFunction: ACPI_SLEEP_FUNCTION,
    ExtendedFunction: ACPI_SLEEP_FUNCTION,
};
pub const ACPI_SLEEP_FUNCTIONS = struct_acpi_sleep_functions;
const struct_unnamed_6 = extern struct {
    Type: ACPI_OBJECT_TYPE,
    Value: UINT64,
};
const struct_unnamed_7 = extern struct {
    Type: ACPI_OBJECT_TYPE,
    Length: UINT32,
    Pointer: [*c]u8,
};
const struct_unnamed_8 = extern struct {
    Type: ACPI_OBJECT_TYPE,
    Length: UINT32,
    Pointer: [*c]UINT8,
};
const struct_unnamed_9 = extern struct {
    Type: ACPI_OBJECT_TYPE,
    Count: UINT32,
    Elements: [*c]union_acpi_object,
};
const struct_unnamed_10 = extern struct {
    Type: ACPI_OBJECT_TYPE,
    ActualType: ACPI_OBJECT_TYPE,
    Handle: ACPI_HANDLE,
};
const struct_unnamed_11 = extern struct {
    Type: ACPI_OBJECT_TYPE,
    ProcId: UINT32,
    PblkAddress: ACPI_IO_ADDRESS,
    PblkLength: UINT32,
};
const struct_unnamed_12 = extern struct {
    Type: ACPI_OBJECT_TYPE,
    SystemLevel: UINT32,
    ResourceOrder: UINT32,
};
pub const union_acpi_object = extern union {
    Type: ACPI_OBJECT_TYPE,
    Integer: struct_unnamed_6,
    String: struct_unnamed_7,
    Buffer: struct_unnamed_8,
    Package: struct_unnamed_9,
    Reference: struct_unnamed_10,
    Processor: struct_unnamed_11,
    PowerResource: struct_unnamed_12,
};
pub const ACPI_OBJECT = union_acpi_object;
pub const struct_acpi_object_list = extern struct {
    Count: UINT32,
    Pointer: [*c]ACPI_OBJECT,
};
pub const ACPI_OBJECT_LIST = struct_acpi_object_list;
pub const struct_acpi_buffer = extern struct {
    Length: ACPI_SIZE,
    Pointer: ?*anyopaque,
};
pub const ACPI_BUFFER = struct_acpi_buffer;
pub const struct_acpi_predefined_names = extern struct {
    Name: [*c]const u8,
    Type: UINT8,
    Val: [*c]u8,
};
pub const ACPI_PREDEFINED_NAMES = struct_acpi_predefined_names;
pub const struct_acpi_system_info = extern struct {
    AcpiCaVersion: UINT32,
    Flags: UINT32,
    TimerResolution: UINT32,
    Reserved1: UINT32,
    Reserved2: UINT32,
    DebugLevel: UINT32,
    DebugLayer: UINT32,
};
pub const ACPI_SYSTEM_INFO = struct_acpi_system_info;
pub const struct_acpi_statistics = extern struct {
    SciCount: UINT32,
    GpeCount: UINT32,
    FixedEventCount: [5]UINT32,
    MethodCount: UINT32,
};
pub const ACPI_STATISTICS = struct_acpi_statistics;
pub const ACPI_OSD_HANDLER = ?*const fn (?*anyopaque) callconv(.C) UINT32;
pub const ACPI_OSD_EXEC_CALLBACK = ?*const fn (?*anyopaque) callconv(.C) void;
pub const ACPI_SCI_HANDLER = ?*const fn (?*anyopaque) callconv(.C) UINT32;
pub const ACPI_GBL_EVENT_HANDLER = ?*const fn (UINT32, ACPI_HANDLE, UINT32, ?*anyopaque) callconv(.C) void;
pub const ACPI_EVENT_HANDLER = ?*const fn (?*anyopaque) callconv(.C) UINT32;
pub const ACPI_GPE_HANDLER = ?*const fn (ACPI_HANDLE, UINT32, ?*anyopaque) callconv(.C) UINT32;
pub const ACPI_NOTIFY_HANDLER = ?*const fn (ACPI_HANDLE, UINT32, ?*anyopaque) callconv(.C) void;
pub const ACPI_OBJECT_HANDLER = ?*const fn (ACPI_HANDLE, ?*anyopaque) callconv(.C) void;
pub const ACPI_INIT_HANDLER = ?*const fn (ACPI_HANDLE, UINT32) callconv(.C) ACPI_STATUS;
pub const ACPI_EXCEPTION_HANDLER = ?*const fn (ACPI_STATUS, ACPI_NAME, UINT16, UINT32, ?*anyopaque) callconv(.C) ACPI_STATUS;
pub const ACPI_TABLE_HANDLER = ?*const fn (UINT32, ?*anyopaque, ?*anyopaque) callconv(.C) ACPI_STATUS;
pub const ACPI_ADR_SPACE_HANDLER = ?*const fn (UINT32, ACPI_PHYSICAL_ADDRESS, UINT32, [*c]UINT64, ?*anyopaque, ?*anyopaque) callconv(.C) ACPI_STATUS;
pub const struct_acpi_connection_info = extern struct {
    Connection: [*c]UINT8,
    Length: UINT16,
    AccessLength: UINT8,
};
pub const ACPI_CONNECTION_INFO = struct_acpi_connection_info;
pub const struct_acpi_pcc_info = extern struct {
    SubspaceId: UINT8,
    Length: UINT16,
    InternalBuffer: [*c]UINT8,
};
pub const ACPI_PCC_INFO = struct_acpi_pcc_info;
pub const struct_acpi_ffh_info = extern struct {
    Offset: UINT64,
    Length: UINT64,
};
pub const ACPI_FFH_INFO = struct_acpi_ffh_info;
pub const ACPI_ADR_SPACE_SETUP = ?*const fn (ACPI_HANDLE, UINT32, ?*anyopaque, [*c]?*anyopaque) callconv(.C) ACPI_STATUS;
pub const ACPI_WALK_CALLBACK = ?*const fn (ACPI_HANDLE, UINT32, ?*anyopaque, [*c]?*anyopaque) callconv(.C) ACPI_STATUS;
pub const ACPI_INTERFACE_HANDLER = ?*const fn (ACPI_STRING, UINT32) callconv(.C) UINT32;
pub const struct_acpi_pnp_device_id = extern struct {
    Length: UINT32,
    String: [*c]u8,
};
pub const ACPI_PNP_DEVICE_ID = struct_acpi_pnp_device_id;
pub const struct_acpi_pnp_device_id_list = extern struct {
    Count: UINT32 align(8),
    ListSize: UINT32,
    pub fn Ids(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), ACPI_PNP_DEVICE_ID) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), ACPI_PNP_DEVICE_ID);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 8)));
    }
};
pub const ACPI_PNP_DEVICE_ID_LIST = struct_acpi_pnp_device_id_list;
pub const struct_acpi_device_info = extern struct {
    InfoSize: UINT32,
    Name: UINT32,
    Type: ACPI_OBJECT_TYPE,
    ParamCount: UINT8,
    Valid: UINT16,
    Flags: UINT8,
    HighestDstates: [4]UINT8,
    LowestDstates: [5]UINT8,
    Address: UINT64,
    HardwareId: ACPI_PNP_DEVICE_ID,
    UniqueId: ACPI_PNP_DEVICE_ID,
    ClassCode: ACPI_PNP_DEVICE_ID,
    CompatibleIdList: ACPI_PNP_DEVICE_ID_LIST,
};
pub const ACPI_DEVICE_INFO = struct_acpi_device_info;
pub const struct_acpi_pci_id = extern struct {
    Segment: UINT16,
    Bus: UINT16,
    Device: UINT16,
    Function: UINT16,
};
pub const ACPI_PCI_ID = struct_acpi_pci_id;
pub const struct_acpi_mem_mapping = extern struct {
    PhysicalAddress: ACPI_PHYSICAL_ADDRESS,
    LogicalAddress: [*c]UINT8,
    Length: ACPI_SIZE,
    NextMm: [*c]struct_acpi_mem_mapping,
};
pub const ACPI_MEM_MAPPING = struct_acpi_mem_mapping;
pub const struct_acpi_mem_space_context = extern struct {
    Length: UINT32,
    Address: ACPI_PHYSICAL_ADDRESS,
    CurMm: [*c]ACPI_MEM_MAPPING,
    FirstMm: [*c]ACPI_MEM_MAPPING,
};
pub const ACPI_MEM_SPACE_CONTEXT = struct_acpi_mem_space_context;
pub const struct_acpi_data_table_mapping = extern struct {
    Pointer: ?*anyopaque,
};
pub const ACPI_DATA_TABLE_MAPPING = struct_acpi_data_table_mapping;
pub const struct_acpi_memory_list = extern struct {
    ListName: [*c]const u8,
    ListHead: ?*anyopaque,
    ObjectSize: UINT16,
    MaxDepth: UINT16,
    CurrentDepth: UINT16,
};
pub const ACPI_MEMORY_LIST = struct_acpi_memory_list;
pub const ACPI_TRACE_AML_METHOD: c_int = 0;
pub const ACPI_TRACE_AML_OPCODE: c_int = 1;
pub const ACPI_TRACE_AML_REGION: c_int = 2;
pub const ACPI_TRACE_EVENT_TYPE = c_uint;
pub const struct_acpi_table_header = extern struct {
    Signature: [4]u8,
    Length: UINT32,
    Revision: UINT8,
    Checksum: UINT8,
    OemId: [6]u8,
    OemTableId: [8]u8,
    OemRevision: UINT32,
    AslCompilerId: [4]u8,
    AslCompilerRevision: UINT32,
};
pub const ACPI_TABLE_HEADER = struct_acpi_table_header;
pub const struct_acpi_generic_address = extern struct {
    SpaceId: UINT8,
    BitWidth: UINT8,
    BitOffset: UINT8,
    AccessWidth: UINT8,
    Address: UINT64,
};
pub const ACPI_GENERIC_ADDRESS = struct_acpi_generic_address;
pub const struct_acpi_table_rsdp = extern struct {
    Signature: [8]u8,
    Checksum: UINT8,
    OemId: [6]u8,
    Revision: UINT8,
    RsdtPhysicalAddress: UINT32,
    Length: UINT32,
    XsdtPhysicalAddress: UINT64,
    ExtendedChecksum: UINT8,
    Reserved: [3]UINT8,
};
pub const ACPI_TABLE_RSDP = struct_acpi_table_rsdp;
pub const struct_acpi_rsdp_common = extern struct {
    Signature: [8]u8,
    Checksum: UINT8,
    OemId: [6]u8,
    Revision: UINT8,
    RsdtPhysicalAddress: UINT32,
};
pub const ACPI_RSDP_COMMON = struct_acpi_rsdp_common;
pub const struct_acpi_rsdp_extension = extern struct {
    Length: UINT32,
    XsdtPhysicalAddress: UINT64,
    ExtendedChecksum: UINT8,
    Reserved: [3]UINT8,
};
pub const ACPI_RSDP_EXTENSION = struct_acpi_rsdp_extension;
pub const struct_acpi_table_rsdt = extern struct {
    Header: ACPI_TABLE_HEADER,
    TableOffsetEntry: [1]UINT32,
};
pub const ACPI_TABLE_RSDT = struct_acpi_table_rsdt;
pub const struct_acpi_table_xsdt = extern struct {
    Header: ACPI_TABLE_HEADER,
    TableOffsetEntry: [1]UINT64,
};
pub const ACPI_TABLE_XSDT = struct_acpi_table_xsdt;
pub const struct_acpi_table_facs = extern struct {
    Signature: [4]u8,
    Length: UINT32,
    HardwareSignature: UINT32,
    FirmwareWakingVector: UINT32,
    GlobalLock: UINT32,
    Flags: UINT32,
    XFirmwareWakingVector: UINT64,
    Version: UINT8,
    Reserved: [3]UINT8,
    OspmFlags: UINT32,
    Reserved1: [24]UINT8,
};
pub const ACPI_TABLE_FACS = struct_acpi_table_facs;
pub const struct_acpi_table_fadt = extern struct {
    Header: ACPI_TABLE_HEADER,
    Facs: UINT32,
    Dsdt: UINT32,
    Model: UINT8,
    PreferredProfile: UINT8,
    SciInterrupt: UINT16,
    SmiCommand: UINT32,
    AcpiEnable: UINT8,
    AcpiDisable: UINT8,
    S4BiosRequest: UINT8,
    PstateControl: UINT8,
    Pm1aEventBlock: UINT32,
    Pm1bEventBlock: UINT32,
    Pm1aControlBlock: UINT32,
    Pm1bControlBlock: UINT32,
    Pm2ControlBlock: UINT32,
    PmTimerBlock: UINT32,
    Gpe0Block: UINT32,
    Gpe1Block: UINT32,
    Pm1EventLength: UINT8,
    Pm1ControlLength: UINT8,
    Pm2ControlLength: UINT8,
    PmTimerLength: UINT8,
    Gpe0BlockLength: UINT8,
    Gpe1BlockLength: UINT8,
    Gpe1Base: UINT8,
    CstControl: UINT8,
    C2Latency: UINT16,
    C3Latency: UINT16,
    FlushSize: UINT16,
    FlushStride: UINT16,
    DutyOffset: UINT8,
    DutyWidth: UINT8,
    DayAlarm: UINT8,
    MonthAlarm: UINT8,
    Century: UINT8,
    BootFlags: UINT16,
    Reserved: UINT8,
    Flags: UINT32,
    ResetRegister: ACPI_GENERIC_ADDRESS,
    ResetValue: UINT8,
    ArmBootFlags: UINT16,
    MinorRevision: UINT8,
    XFacs: UINT64,
    XDsdt: UINT64,
    XPm1aEventBlock: ACPI_GENERIC_ADDRESS,
    XPm1bEventBlock: ACPI_GENERIC_ADDRESS,
    XPm1aControlBlock: ACPI_GENERIC_ADDRESS,
    XPm1bControlBlock: ACPI_GENERIC_ADDRESS,
    XPm2ControlBlock: ACPI_GENERIC_ADDRESS,
    XPmTimerBlock: ACPI_GENERIC_ADDRESS,
    XGpe0Block: ACPI_GENERIC_ADDRESS,
    XGpe1Block: ACPI_GENERIC_ADDRESS,
    SleepControl: ACPI_GENERIC_ADDRESS,
    SleepStatus: ACPI_GENERIC_ADDRESS,
    HypervisorId: UINT64,
};
pub const ACPI_TABLE_FADT = struct_acpi_table_fadt;
pub const PM_UNSPECIFIED: c_int = 0;
pub const PM_DESKTOP: c_int = 1;
pub const PM_MOBILE: c_int = 2;
pub const PM_WORKSTATION: c_int = 3;
pub const PM_ENTERPRISE_SERVER: c_int = 4;
pub const PM_SOHO_SERVER: c_int = 5;
pub const PM_APPLIANCE_PC: c_int = 6;
pub const PM_PERFORMANCE_SERVER: c_int = 7;
pub const PM_TABLET: c_int = 8;
pub const enum_AcpiPreferredPmProfiles = c_uint;
pub const union_acpi_name_union = extern union {
    Integer: UINT32,
    Ascii: [4]u8,
};
pub const ACPI_NAME_UNION = union_acpi_name_union;
pub const struct_acpi_table_desc = extern struct {
    Address: ACPI_PHYSICAL_ADDRESS,
    Pointer: [*c]ACPI_TABLE_HEADER,
    Length: UINT32,
    Signature: ACPI_NAME_UNION,
    OwnerId: ACPI_OWNER_ID,
    Flags: UINT8,
    ValidationCount: UINT16,
};
pub const ACPI_TABLE_DESC = struct_acpi_table_desc;
pub const struct_acpi_subtable_header = extern struct {
    Type: UINT8,
    Length: UINT8,
};
pub const ACPI_SUBTABLE_HEADER = struct_acpi_subtable_header;
pub const struct_acpi_whea_header = extern struct {
    Action: UINT8,
    Instruction: UINT8,
    Flags: UINT8,
    Reserved: UINT8,
    RegisterRegion: ACPI_GENERIC_ADDRESS,
    Value: UINT64,
    Mask: UINT64,
};
pub const ACPI_WHEA_HEADER = struct_acpi_whea_header;
pub const struct_acpi_table_asf = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_ASF = struct_acpi_table_asf;
pub const struct_acpi_asf_header = extern struct {
    Type: UINT8,
    Reserved: UINT8,
    Length: UINT16,
};
pub const ACPI_ASF_HEADER = struct_acpi_asf_header;
pub const ACPI_ASF_TYPE_INFO: c_int = 0;
pub const ACPI_ASF_TYPE_ALERT: c_int = 1;
pub const ACPI_ASF_TYPE_CONTROL: c_int = 2;
pub const ACPI_ASF_TYPE_BOOT: c_int = 3;
pub const ACPI_ASF_TYPE_ADDRESS: c_int = 4;
pub const ACPI_ASF_TYPE_RESERVED: c_int = 5;
pub const enum_AcpiAsfType = c_uint;
pub const struct_acpi_asf_info = extern struct {
    Header: ACPI_ASF_HEADER,
    MinResetValue: UINT8,
    MinPollInterval: UINT8,
    SystemId: UINT16,
    MfgId: UINT32,
    Flags: UINT8,
    Reserved2: [3]UINT8,
};
pub const ACPI_ASF_INFO = struct_acpi_asf_info;
pub const struct_acpi_asf_alert = extern struct {
    Header: ACPI_ASF_HEADER,
    AssertMask: UINT8,
    DeassertMask: UINT8,
    Alerts: UINT8,
    DataLength: UINT8,
};
pub const ACPI_ASF_ALERT = struct_acpi_asf_alert;
pub const struct_acpi_asf_alert_data = extern struct {
    Address: UINT8,
    Command: UINT8,
    Mask: UINT8,
    Value: UINT8,
    SensorType: UINT8,
    Type: UINT8,
    Offset: UINT8,
    SourceType: UINT8,
    Severity: UINT8,
    SensorNumber: UINT8,
    Entity: UINT8,
    Instance: UINT8,
};
pub const ACPI_ASF_ALERT_DATA = struct_acpi_asf_alert_data;
pub const struct_acpi_asf_remote = extern struct {
    Header: ACPI_ASF_HEADER,
    Controls: UINT8,
    DataLength: UINT8,
    Reserved2: UINT16,
};
pub const ACPI_ASF_REMOTE = struct_acpi_asf_remote;
pub const struct_acpi_asf_control_data = extern struct {
    Function: UINT8,
    Address: UINT8,
    Command: UINT8,
    Value: UINT8,
};
pub const ACPI_ASF_CONTROL_DATA = struct_acpi_asf_control_data;
pub const struct_acpi_asf_rmcp = extern struct {
    Header: ACPI_ASF_HEADER,
    Capabilities: [7]UINT8,
    CompletionCode: UINT8,
    EnterpriseId: UINT32,
    Command: UINT8,
    Parameter: UINT16,
    BootOptions: UINT16,
    OemParameters: UINT16,
};
pub const ACPI_ASF_RMCP = struct_acpi_asf_rmcp;
pub const struct_acpi_asf_address = extern struct {
    Header: ACPI_ASF_HEADER,
    EpromAddress: UINT8,
    Devices: UINT8,
};
pub const ACPI_ASF_ADDRESS = struct_acpi_asf_address;
pub const struct_acpi_table_aspt = extern struct {
    Header: ACPI_TABLE_HEADER,
    NumEntries: UINT32,
};
pub const ACPI_TABLE_ASPT = struct_acpi_table_aspt;
pub const struct_acpi_aspt_header = extern struct {
    Type: UINT16,
    Length: UINT16,
};
pub const ACPI_ASPT_HEADER = struct_acpi_aspt_header;
pub const ACPI_ASPT_TYPE_GLOBAL_REGS: c_int = 0;
pub const ACPI_ASPT_TYPE_SEV_MBOX_REGS: c_int = 1;
pub const ACPI_ASPT_TYPE_ACPI_MBOX_REGS: c_int = 2;
pub const ACPI_ASPT_TYPE_UNKNOWN: c_int = 3;
pub const enum_AcpiAsptType = c_uint;
pub const struct_acpi_aspt_global_regs = extern struct {
    Header: ACPI_ASPT_HEADER,
    Reserved: UINT32,
    FeatureRegAddr: UINT64,
    IrqEnRegAddr: UINT64,
    IrqStRegAddr: UINT64,
};
pub const ACPI_ASPT_GLOBAL_REGS = struct_acpi_aspt_global_regs;
pub const struct_acpi_aspt_sev_mbox_regs = extern struct {
    Header: ACPI_ASPT_HEADER,
    MboxIrqId: UINT8,
    Reserved: [3]UINT8,
    CmdRespRegAddr: UINT64,
    CmdBufLoRegAddr: UINT64,
    CmdBufHiRegAddr: UINT64,
};
pub const ACPI_ASPT_SEV_MBOX_REGS = struct_acpi_aspt_sev_mbox_regs;
pub const struct_acpi_aspt_acpi_mbox_regs = extern struct {
    Header: ACPI_ASPT_HEADER,
    Reserved1: UINT32,
    CmdRespRegAddr: UINT64,
    Reserved2: [2]UINT64,
};
pub const ACPI_ASPT_ACPI_MBOX_REGS = struct_acpi_aspt_acpi_mbox_regs;
pub const struct_acpi_table_bert = extern struct {
    Header: ACPI_TABLE_HEADER,
    RegionLength: UINT32,
    Address: UINT64,
};
pub const ACPI_TABLE_BERT = struct_acpi_table_bert;
pub const struct_acpi_bert_region = extern struct {
    BlockStatus: UINT32,
    RawDataOffset: UINT32,
    RawDataLength: UINT32,
    DataLength: UINT32,
    ErrorSeverity: UINT32,
};
pub const ACPI_BERT_REGION = struct_acpi_bert_region;
pub const ACPI_BERT_ERROR_CORRECTABLE: c_int = 0;
pub const ACPI_BERT_ERROR_FATAL: c_int = 1;
pub const ACPI_BERT_ERROR_CORRECTED: c_int = 2;
pub const ACPI_BERT_ERROR_NONE: c_int = 3;
pub const ACPI_BERT_ERROR_RESERVED: c_int = 4;
pub const enum_AcpiBertErrorSeverity = c_uint;
pub const struct_acpi_table_bgrt = extern struct {
    Header: ACPI_TABLE_HEADER,
    Version: UINT16,
    Status: UINT8,
    ImageType: UINT8,
    ImageAddress: UINT64,
    ImageOffsetX: UINT32,
    ImageOffsetY: UINT32,
};
pub const ACPI_TABLE_BGRT = struct_acpi_table_bgrt;
pub const struct_acpi_table_boot = extern struct {
    Header: ACPI_TABLE_HEADER,
    CmosIndex: UINT8,
    Reserved: [3]UINT8,
};
pub const ACPI_TABLE_BOOT = struct_acpi_table_boot;
pub const struct_acpi_table_cdat = extern struct {
    Length: UINT32,
    Revision: UINT8,
    Checksum: UINT8,
    Reserved: [6]UINT8,
    Sequence: UINT32,
};
pub const ACPI_TABLE_CDAT = struct_acpi_table_cdat;
pub const struct_acpi_cdat_header = extern struct {
    Type: UINT8,
    Reserved: UINT8,
    Length: UINT16,
};
pub const ACPI_CDAT_HEADER = struct_acpi_cdat_header;
pub const ACPI_CDAT_TYPE_DSMAS: c_int = 0;
pub const ACPI_CDAT_TYPE_DSLBIS: c_int = 1;
pub const ACPI_CDAT_TYPE_DSMSCIS: c_int = 2;
pub const ACPI_CDAT_TYPE_DSIS: c_int = 3;
pub const ACPI_CDAT_TYPE_DSEMTS: c_int = 4;
pub const ACPI_CDAT_TYPE_SSLBIS: c_int = 5;
pub const ACPI_CDAT_TYPE_RESERVED: c_int = 6;
pub const enum_AcpiCdatType = c_uint;
pub const struct_acpi_cdat_dsmas = extern struct {
    DsmadHandle: UINT8,
    Flags: UINT8,
    Reserved: UINT16,
    DpaBaseAddress: UINT64,
    DpaLength: UINT64,
};
pub const ACPI_CDAT_DSMAS = struct_acpi_cdat_dsmas;
pub const struct_acpi_cdat_dslbis = extern struct {
    Handle: UINT8,
    Flags: UINT8,
    DataType: UINT8,
    Reserved: UINT8,
    EntryBaseUnit: UINT64,
    Entry: [3]UINT16,
    Reserved2: UINT16,
};
pub const ACPI_CDAT_DSLBIS = struct_acpi_cdat_dslbis;
pub const struct_acpi_cdat_dsmscis = extern struct {
    DsmasHandle: UINT8,
    Reserved: [3]UINT8,
    SideCacheSize: UINT64,
    CacheAttributes: UINT32,
};
pub const ACPI_CDAT_DSMSCIS = struct_acpi_cdat_dsmscis;
pub const struct_acpi_cdat_dsis = extern struct {
    Flags: UINT8,
    Handle: UINT8,
    Reserved: UINT16,
};
pub const ACPI_CDAT_DSIS = struct_acpi_cdat_dsis;
pub const struct_acpi_cdat_dsemts = extern struct {
    DsmasHandle: UINT8,
    MemoryType: UINT8,
    Reserved: UINT16,
    DpaOffset: UINT64,
    RangeLength: UINT64,
};
pub const ACPI_CDAT_DSEMTS = struct_acpi_cdat_dsemts;
pub const struct_acpi_cdat_sslbis = extern struct {
    DataType: UINT8,
    Reserved: [3]UINT8,
    EntryBaseUnit: UINT64,
};
pub const ACPI_CDAT_SSLBIS = struct_acpi_cdat_sslbis;
pub const struct_acpi_cdat_sslbe = extern struct {
    PortxId: UINT16,
    PortyId: UINT16,
    LatencyOrBandwidth: UINT16,
    Reserved: UINT16,
};
pub const ACPI_CDAT_SSLBE = struct_acpi_cdat_sslbe;
pub const struct_acpi_table_cedt = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_CEDT = struct_acpi_table_cedt;
pub const struct_acpi_cedt_header = extern struct {
    Type: UINT8,
    Reserved: UINT8,
    Length: UINT16,
};
pub const ACPI_CEDT_HEADER = struct_acpi_cedt_header;
pub const ACPI_CEDT_TYPE_CHBS: c_int = 0;
pub const ACPI_CEDT_TYPE_CFMWS: c_int = 1;
pub const ACPI_CEDT_TYPE_CXIMS: c_int = 2;
pub const ACPI_CEDT_TYPE_RDPAS: c_int = 3;
pub const ACPI_CEDT_TYPE_RESERVED: c_int = 4;
pub const enum_AcpiCedtType = c_uint;
pub const struct_acpi_cedt_chbs = extern struct {
    Header: ACPI_CEDT_HEADER,
    Uid: UINT32,
    CxlVersion: UINT32,
    Reserved: UINT32,
    Base: UINT64,
    Length: UINT64,
};
pub const ACPI_CEDT_CHBS = struct_acpi_cedt_chbs;
pub const struct_acpi_cedt_cfmws = extern struct {
    Header: ACPI_CEDT_HEADER align(1),
    Reserved1: UINT32,
    BaseHpa: UINT64,
    WindowSize: UINT64,
    InterleaveWays: UINT8,
    InterleaveArithmetic: UINT8,
    Reserved2: UINT16,
    Granularity: UINT32,
    Restrictions: UINT16,
    QtgId: UINT16,
    pub fn InterleaveTargets(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT32) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT32);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 36)));
    }
};
pub const ACPI_CEDT_CFMWS = struct_acpi_cedt_cfmws;
pub const struct_acpi_cedt_cfmws_target_element = extern struct {
    InterleaveTarget: UINT32,
};
pub const ACPI_CEDT_CFMWS_TARGET_ELEMENT = struct_acpi_cedt_cfmws_target_element;
pub const struct_acpi_cedt_cxims = extern struct {
    Header: ACPI_CEDT_HEADER align(1),
    Reserved1: UINT16,
    Hbig: UINT8,
    NrXormaps: UINT8,
    pub fn XormapList(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT64) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT64);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 8)));
    }
};
pub const struct_acpi_cedt_rdpas = extern struct {
    Header: ACPI_CEDT_HEADER,
    Reserved1: UINT8,
    Length: UINT16,
    Segment: UINT16,
    Bdf: UINT16,
    Protocol: UINT8,
    Address: UINT64,
};
pub const struct_acpi_table_cpep = extern struct {
    Header: ACPI_TABLE_HEADER,
    Reserved: UINT64,
};
pub const ACPI_TABLE_CPEP = struct_acpi_table_cpep;
pub const struct_acpi_cpep_polling = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Id: UINT8,
    Eid: UINT8,
    Interval: UINT32,
};
pub const ACPI_CPEP_POLLING = struct_acpi_cpep_polling;
pub const struct_acpi_table_csrt = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_CSRT = struct_acpi_table_csrt;
pub const struct_acpi_csrt_group = extern struct {
    Length: UINT32,
    VendorId: UINT32,
    SubvendorId: UINT32,
    DeviceId: UINT16,
    SubdeviceId: UINT16,
    Revision: UINT16,
    Reserved: UINT16,
    SharedInfoLength: UINT32,
};
pub const ACPI_CSRT_GROUP = struct_acpi_csrt_group;
pub const struct_acpi_csrt_shared_info = extern struct {
    MajorVersion: UINT16,
    MinorVersion: UINT16,
    MmioBaseLow: UINT32,
    MmioBaseHigh: UINT32,
    GsiInterrupt: UINT32,
    InterruptPolarity: UINT8,
    InterruptMode: UINT8,
    NumChannels: UINT8,
    DmaAddressWidth: UINT8,
    BaseRequestLine: UINT16,
    NumHandshakeSignals: UINT16,
    MaxBlockSize: UINT32,
};
pub const ACPI_CSRT_SHARED_INFO = struct_acpi_csrt_shared_info;
pub const struct_acpi_csrt_descriptor = extern struct {
    Length: UINT32,
    Type: UINT16,
    Subtype: UINT16,
    Uid: UINT32,
};
pub const ACPI_CSRT_DESCRIPTOR = struct_acpi_csrt_descriptor;
pub const struct_acpi_table_dbg2 = extern struct {
    Header: ACPI_TABLE_HEADER,
    InfoOffset: UINT32,
    InfoCount: UINT32,
};
pub const ACPI_TABLE_DBG2 = struct_acpi_table_dbg2;
pub const struct_acpi_dbg2_header = extern struct {
    InfoOffset: UINT32,
    InfoCount: UINT32,
};
pub const ACPI_DBG2_HEADER = struct_acpi_dbg2_header;
pub const struct_acpi_dbg2_device = extern struct {
    Revision: UINT8,
    Length: UINT16,
    RegisterCount: UINT8,
    NamepathLength: UINT16,
    NamepathOffset: UINT16,
    OemDataLength: UINT16,
    OemDataOffset: UINT16,
    PortType: UINT16,
    PortSubtype: UINT16,
    Reserved: UINT16,
    BaseAddressOffset: UINT16,
    AddressSizeOffset: UINT16,
};
pub const ACPI_DBG2_DEVICE = struct_acpi_dbg2_device;
pub const struct_acpi_table_dbgp = extern struct {
    Header: ACPI_TABLE_HEADER,
    Type: UINT8,
    Reserved: [3]UINT8,
    DebugPort: ACPI_GENERIC_ADDRESS,
};
pub const ACPI_TABLE_DBGP = struct_acpi_table_dbgp;
pub const struct_acpi_table_dmar = extern struct {
    Header: ACPI_TABLE_HEADER,
    Width: UINT8,
    Flags: UINT8,
    Reserved: [10]UINT8,
};
pub const ACPI_TABLE_DMAR = struct_acpi_table_dmar;
pub const struct_acpi_dmar_header = extern struct {
    Type: UINT16,
    Length: UINT16,
};
pub const ACPI_DMAR_HEADER = struct_acpi_dmar_header;
pub const ACPI_DMAR_TYPE_HARDWARE_UNIT: c_int = 0;
pub const ACPI_DMAR_TYPE_RESERVED_MEMORY: c_int = 1;
pub const ACPI_DMAR_TYPE_ROOT_ATS: c_int = 2;
pub const ACPI_DMAR_TYPE_HARDWARE_AFFINITY: c_int = 3;
pub const ACPI_DMAR_TYPE_NAMESPACE: c_int = 4;
pub const ACPI_DMAR_TYPE_SATC: c_int = 5;
pub const ACPI_DMAR_TYPE_RESERVED: c_int = 6;
pub const enum_AcpiDmarType = c_uint;
pub const struct_acpi_dmar_device_scope = extern struct {
    EntryType: UINT8,
    Length: UINT8,
    Reserved: UINT16,
    EnumerationId: UINT8,
    Bus: UINT8,
};
pub const ACPI_DMAR_DEVICE_SCOPE = struct_acpi_dmar_device_scope;
pub const ACPI_DMAR_SCOPE_TYPE_NOT_USED: c_int = 0;
pub const ACPI_DMAR_SCOPE_TYPE_ENDPOINT: c_int = 1;
pub const ACPI_DMAR_SCOPE_TYPE_BRIDGE: c_int = 2;
pub const ACPI_DMAR_SCOPE_TYPE_IOAPIC: c_int = 3;
pub const ACPI_DMAR_SCOPE_TYPE_HPET: c_int = 4;
pub const ACPI_DMAR_SCOPE_TYPE_NAMESPACE: c_int = 5;
pub const ACPI_DMAR_SCOPE_TYPE_RESERVED: c_int = 6;
pub const enum_AcpiDmarScopeType = c_uint;
pub const struct_acpi_dmar_pci_path = extern struct {
    Device: UINT8,
    Function: UINT8,
};
pub const ACPI_DMAR_PCI_PATH = struct_acpi_dmar_pci_path;
pub const struct_acpi_dmar_hardware_unit = extern struct {
    Header: ACPI_DMAR_HEADER,
    Flags: UINT8,
    Reserved: UINT8,
    Segment: UINT16,
    Address: UINT64,
};
pub const ACPI_DMAR_HARDWARE_UNIT = struct_acpi_dmar_hardware_unit;
pub const struct_acpi_dmar_reserved_memory = extern struct {
    Header: ACPI_DMAR_HEADER,
    Reserved: UINT16,
    Segment: UINT16,
    BaseAddress: UINT64,
    EndAddress: UINT64,
};
pub const ACPI_DMAR_RESERVED_MEMORY = struct_acpi_dmar_reserved_memory;
pub const struct_acpi_dmar_atsr = extern struct {
    Header: ACPI_DMAR_HEADER,
    Flags: UINT8,
    Reserved: UINT8,
    Segment: UINT16,
};
pub const ACPI_DMAR_ATSR = struct_acpi_dmar_atsr;
pub const struct_acpi_dmar_rhsa = extern struct {
    Header: ACPI_DMAR_HEADER,
    Reserved: UINT32,
    BaseAddress: UINT64,
    ProximityDomain: UINT32,
};
pub const ACPI_DMAR_RHSA = struct_acpi_dmar_rhsa;
const struct_unnamed_15 = extern struct {};
const struct_unnamed_14 = extern struct {
    __Empty_DeviceName: struct_unnamed_15 align(1),
    pub fn DeviceName(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 0)));
    }
};
const union_unnamed_13 = extern union {
    __pad: u8,
    unnamed_0: struct_unnamed_14,
};
pub const struct_acpi_dmar_andd = extern struct {
    Header: ACPI_DMAR_HEADER,
    Reserved: [3]UINT8,
    DeviceNumber: UINT8,
    unnamed_0: union_unnamed_13,
};
pub const ACPI_DMAR_ANDD = struct_acpi_dmar_andd;
pub const struct_acpi_dmar_satc = extern struct {
    Header: ACPI_DMAR_HEADER,
    Flags: UINT8,
    Reserved: UINT8,
    Segment: UINT16,
};
pub const ACPI_DMAR_SATC = struct_acpi_dmar_satc;
pub const struct_acpi_table_drtm = extern struct {
    Header: ACPI_TABLE_HEADER,
    EntryBaseAddress: UINT64,
    EntryLength: UINT64,
    EntryAddress32: UINT32,
    EntryAddress64: UINT64,
    ExitAddress: UINT64,
    LogAreaAddress: UINT64,
    LogAreaLength: UINT32,
    ArchDependentAddress: UINT64,
    Flags: UINT32,
};
pub const ACPI_TABLE_DRTM = struct_acpi_table_drtm;
pub const struct_acpi_drtm_vtable_list = extern struct {
    ValidatedTableCount: UINT32 align(1),
    pub fn ValidatedTables(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT64) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT64);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 4)));
    }
};
pub const ACPI_DRTM_VTABLE_LIST = struct_acpi_drtm_vtable_list;
pub const struct_acpi_drtm_resource = extern struct {
    Size: [7]UINT8,
    Type: UINT8,
    Address: UINT64,
};
pub const ACPI_DRTM_RESOURCE = struct_acpi_drtm_resource;
pub const struct_acpi_drtm_resource_list = extern struct {
    ResourceCount: UINT32 align(1),
    pub fn Resources(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), ACPI_DRTM_RESOURCE) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), ACPI_DRTM_RESOURCE);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 4)));
    }
};
pub const ACPI_DRTM_RESOURCE_LIST = struct_acpi_drtm_resource_list;
pub const struct_acpi_drtm_dps_id = extern struct {
    DpsIdLength: UINT32,
    DpsId: [16]UINT8,
};
pub const ACPI_DRTM_DPS_ID = struct_acpi_drtm_dps_id;
pub const struct_acpi_table_ecdt = extern struct {
    Header: ACPI_TABLE_HEADER align(1),
    Control: ACPI_GENERIC_ADDRESS,
    Data: ACPI_GENERIC_ADDRESS,
    Uid: UINT32,
    Gpe: UINT8,
    pub fn Id(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 65)));
    }
};
pub const ACPI_TABLE_ECDT = struct_acpi_table_ecdt;
pub const struct_acpi_table_einj = extern struct {
    Header: ACPI_TABLE_HEADER,
    HeaderLength: UINT32,
    Flags: UINT8,
    Reserved: [3]UINT8,
    Entries: UINT32,
};
pub const ACPI_TABLE_EINJ = struct_acpi_table_einj;
pub const struct_acpi_einj_entry = extern struct {
    WheaHeader: ACPI_WHEA_HEADER,
};
pub const ACPI_EINJ_ENTRY = struct_acpi_einj_entry;
pub const ACPI_EINJ_BEGIN_OPERATION: c_int = 0;
pub const ACPI_EINJ_GET_TRIGGER_TABLE: c_int = 1;
pub const ACPI_EINJ_SET_ERROR_TYPE: c_int = 2;
pub const ACPI_EINJ_GET_ERROR_TYPE: c_int = 3;
pub const ACPI_EINJ_END_OPERATION: c_int = 4;
pub const ACPI_EINJ_EXECUTE_OPERATION: c_int = 5;
pub const ACPI_EINJ_CHECK_BUSY_STATUS: c_int = 6;
pub const ACPI_EINJ_GET_COMMAND_STATUS: c_int = 7;
pub const ACPI_EINJ_SET_ERROR_TYPE_WITH_ADDRESS: c_int = 8;
pub const ACPI_EINJ_GET_EXECUTE_TIMINGS: c_int = 9;
pub const ACPI_EINJ_ACTION_RESERVED: c_int = 10;
pub const ACPI_EINJ_TRIGGER_ERROR: c_int = 255;
pub const enum_AcpiEinjActions = c_uint;
pub const ACPI_EINJ_READ_REGISTER: c_int = 0;
pub const ACPI_EINJ_READ_REGISTER_VALUE: c_int = 1;
pub const ACPI_EINJ_WRITE_REGISTER: c_int = 2;
pub const ACPI_EINJ_WRITE_REGISTER_VALUE: c_int = 3;
pub const ACPI_EINJ_NOOP: c_int = 4;
pub const ACPI_EINJ_FLUSH_CACHELINE: c_int = 5;
pub const ACPI_EINJ_INSTRUCTION_RESERVED: c_int = 6;
pub const enum_AcpiEinjInstructions = c_uint;
pub const struct_acpi_einj_error_type_with_addr = extern struct {
    ErrorType: UINT32,
    VendorStructOffset: UINT32,
    Flags: UINT32,
    ApicId: UINT32,
    Address: UINT64,
    Range: UINT64,
    PcieId: UINT32,
};
pub const ACPI_EINJ_ERROR_TYPE_WITH_ADDR = struct_acpi_einj_error_type_with_addr;
pub const struct_acpi_einj_vendor = extern struct {
    Length: UINT32,
    PcieId: UINT32,
    VendorId: UINT16,
    DeviceId: UINT16,
    RevisionId: UINT8,
    Reserved: [3]UINT8,
};
pub const ACPI_EINJ_VENDOR = struct_acpi_einj_vendor;
pub const struct_acpi_einj_trigger = extern struct {
    HeaderSize: UINT32,
    Revision: UINT32,
    TableSize: UINT32,
    EntryCount: UINT32,
};
pub const ACPI_EINJ_TRIGGER = struct_acpi_einj_trigger;
pub const ACPI_EINJ_SUCCESS: c_int = 0;
pub const ACPI_EINJ_FAILURE: c_int = 1;
pub const ACPI_EINJ_INVALID_ACCESS: c_int = 2;
pub const ACPI_EINJ_STATUS_RESERVED: c_int = 3;
pub const enum_AcpiEinjCommandStatus = c_uint;
pub const struct_acpi_table_erst = extern struct {
    Header: ACPI_TABLE_HEADER,
    HeaderLength: UINT32,
    Reserved: UINT32,
    Entries: UINT32,
};
pub const ACPI_TABLE_ERST = struct_acpi_table_erst;
pub const struct_acpi_erst_entry = extern struct {
    WheaHeader: ACPI_WHEA_HEADER,
};
pub const ACPI_ERST_ENTRY = struct_acpi_erst_entry;
pub const ACPI_ERST_BEGIN_WRITE: c_int = 0;
pub const ACPI_ERST_BEGIN_READ: c_int = 1;
pub const ACPI_ERST_BEGIN_CLEAR: c_int = 2;
pub const ACPI_ERST_END: c_int = 3;
pub const ACPI_ERST_SET_RECORD_OFFSET: c_int = 4;
pub const ACPI_ERST_EXECUTE_OPERATION: c_int = 5;
pub const ACPI_ERST_CHECK_BUSY_STATUS: c_int = 6;
pub const ACPI_ERST_GET_COMMAND_STATUS: c_int = 7;
pub const ACPI_ERST_GET_RECORD_ID: c_int = 8;
pub const ACPI_ERST_SET_RECORD_ID: c_int = 9;
pub const ACPI_ERST_GET_RECORD_COUNT: c_int = 10;
pub const ACPI_ERST_BEGIN_DUMMY_WRIITE: c_int = 11;
pub const ACPI_ERST_NOT_USED: c_int = 12;
pub const ACPI_ERST_GET_ERROR_RANGE: c_int = 13;
pub const ACPI_ERST_GET_ERROR_LENGTH: c_int = 14;
pub const ACPI_ERST_GET_ERROR_ATTRIBUTES: c_int = 15;
pub const ACPI_ERST_EXECUTE_TIMINGS: c_int = 16;
pub const ACPI_ERST_ACTION_RESERVED: c_int = 17;
pub const enum_AcpiErstActions = c_uint;
pub const ACPI_ERST_READ_REGISTER: c_int = 0;
pub const ACPI_ERST_READ_REGISTER_VALUE: c_int = 1;
pub const ACPI_ERST_WRITE_REGISTER: c_int = 2;
pub const ACPI_ERST_WRITE_REGISTER_VALUE: c_int = 3;
pub const ACPI_ERST_NOOP: c_int = 4;
pub const ACPI_ERST_LOAD_VAR1: c_int = 5;
pub const ACPI_ERST_LOAD_VAR2: c_int = 6;
pub const ACPI_ERST_STORE_VAR1: c_int = 7;
pub const ACPI_ERST_ADD: c_int = 8;
pub const ACPI_ERST_SUBTRACT: c_int = 9;
pub const ACPI_ERST_ADD_VALUE: c_int = 10;
pub const ACPI_ERST_SUBTRACT_VALUE: c_int = 11;
pub const ACPI_ERST_STALL: c_int = 12;
pub const ACPI_ERST_STALL_WHILE_TRUE: c_int = 13;
pub const ACPI_ERST_SKIP_NEXT_IF_TRUE: c_int = 14;
pub const ACPI_ERST_GOTO: c_int = 15;
pub const ACPI_ERST_SET_SRC_ADDRESS_BASE: c_int = 16;
pub const ACPI_ERST_SET_DST_ADDRESS_BASE: c_int = 17;
pub const ACPI_ERST_MOVE_DATA: c_int = 18;
pub const ACPI_ERST_INSTRUCTION_RESERVED: c_int = 19;
pub const enum_AcpiErstInstructions = c_uint;
pub const ACPI_ERST_SUCCESS: c_int = 0;
pub const ACPI_ERST_NO_SPACE: c_int = 1;
pub const ACPI_ERST_NOT_AVAILABLE: c_int = 2;
pub const ACPI_ERST_FAILURE: c_int = 3;
pub const ACPI_ERST_RECORD_EMPTY: c_int = 4;
pub const ACPI_ERST_NOT_FOUND: c_int = 5;
pub const ACPI_ERST_STATUS_RESERVED: c_int = 6;
pub const enum_AcpiErstCommandStatus = c_uint;
pub const struct_acpi_erst_info = extern struct {
    Signature: UINT16,
    Data: [48]UINT8,
};
pub const ACPI_ERST_INFO = struct_acpi_erst_info;
pub const struct_acpi_table_fpdt = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_FPDT = struct_acpi_table_fpdt;
pub const struct_acpi_fpdt_header = extern struct {
    Type: UINT16,
    Length: UINT8,
    Revision: UINT8,
};
pub const ACPI_FPDT_HEADER = struct_acpi_fpdt_header;
pub const ACPI_FPDT_TYPE_BOOT: c_int = 0;
pub const ACPI_FPDT_TYPE_S3PERF: c_int = 1;
pub const enum_AcpiFpdtType = c_uint;
pub const struct_acpi_fpdt_boot_pointer = extern struct {
    Header: ACPI_FPDT_HEADER,
    Reserved: [4]UINT8,
    Address: UINT64,
};
pub const ACPI_FPDT_BOOT_POINTER = struct_acpi_fpdt_boot_pointer;
pub const struct_acpi_fpdt_s3pt_pointer = extern struct {
    Header: ACPI_FPDT_HEADER,
    Reserved: [4]UINT8,
    Address: UINT64,
};
pub const ACPI_FPDT_S3PT_POINTER = struct_acpi_fpdt_s3pt_pointer;
pub const struct_acpi_table_s3pt = extern struct {
    Signature: [4]UINT8,
    Length: UINT32,
};
pub const ACPI_TABLE_S3PT = struct_acpi_table_s3pt;
pub const ACPI_S3PT_TYPE_RESUME: c_int = 0;
pub const ACPI_S3PT_TYPE_SUSPEND: c_int = 1;
pub const ACPI_FPDT_BOOT_PERFORMANCE: c_int = 2;
pub const enum_AcpiS3ptType = c_uint;
pub const struct_acpi_s3pt_resume = extern struct {
    Header: ACPI_FPDT_HEADER,
    ResumeCount: UINT32,
    FullResume: UINT64,
    AverageResume: UINT64,
};
pub const ACPI_S3PT_RESUME = struct_acpi_s3pt_resume;
pub const struct_acpi_s3pt_suspend = extern struct {
    Header: ACPI_FPDT_HEADER,
    SuspendStart: UINT64,
    SuspendEnd: UINT64,
};
pub const ACPI_S3PT_SUSPEND = struct_acpi_s3pt_suspend;
pub const struct_acpi_fpdt_boot = extern struct {
    Header: ACPI_FPDT_HEADER,
    Reserved: [4]UINT8,
    ResetEnd: UINT64,
    LoadStart: UINT64,
    StartupStart: UINT64,
    ExitServicesEntry: UINT64,
    ExitServicesExit: UINT64,
};
pub const ACPI_FPDT_BOOT = struct_acpi_fpdt_boot;
pub const struct_acpi_table_gtdt = extern struct {
    Header: ACPI_TABLE_HEADER,
    CounterBlockAddresss: UINT64,
    Reserved: UINT32,
    SecureEl1Interrupt: UINT32,
    SecureEl1Flags: UINT32,
    NonSecureEl1Interrupt: UINT32,
    NonSecureEl1Flags: UINT32,
    VirtualTimerInterrupt: UINT32,
    VirtualTimerFlags: UINT32,
    NonSecureEl2Interrupt: UINT32,
    NonSecureEl2Flags: UINT32,
    CounterReadBlockAddress: UINT64,
    PlatformTimerCount: UINT32,
    PlatformTimerOffset: UINT32,
};
pub const ACPI_TABLE_GTDT = struct_acpi_table_gtdt;
pub const struct_acpi_gtdt_el2 = extern struct {
    VirtualEL2TimerGsiv: UINT32,
    VirtualEL2TimerFlags: UINT32,
};
pub const ACPI_GTDT_EL2 = struct_acpi_gtdt_el2;
pub const struct_acpi_gtdt_header = extern struct {
    Type: UINT8,
    Length: UINT16,
};
pub const ACPI_GTDT_HEADER = struct_acpi_gtdt_header;
pub const ACPI_GTDT_TYPE_TIMER_BLOCK: c_int = 0;
pub const ACPI_GTDT_TYPE_WATCHDOG: c_int = 1;
pub const ACPI_GTDT_TYPE_RESERVED: c_int = 2;
pub const enum_AcpiGtdtType = c_uint;
pub const struct_acpi_gtdt_timer_block = extern struct {
    Header: ACPI_GTDT_HEADER,
    Reserved: UINT8,
    BlockAddress: UINT64,
    TimerCount: UINT32,
    TimerOffset: UINT32,
};
pub const ACPI_GTDT_TIMER_BLOCK = struct_acpi_gtdt_timer_block;
pub const struct_acpi_gtdt_timer_entry = extern struct {
    FrameNumber: UINT8,
    Reserved: [3]UINT8,
    BaseAddress: UINT64,
    El0BaseAddress: UINT64,
    TimerInterrupt: UINT32,
    TimerFlags: UINT32,
    VirtualTimerInterrupt: UINT32,
    VirtualTimerFlags: UINT32,
    CommonFlags: UINT32,
};
pub const ACPI_GTDT_TIMER_ENTRY = struct_acpi_gtdt_timer_entry;
pub const struct_acpi_gtdt_watchdog = extern struct {
    Header: ACPI_GTDT_HEADER,
    Reserved: UINT8,
    RefreshFrameAddress: UINT64,
    ControlFrameAddress: UINT64,
    TimerInterrupt: UINT32,
    TimerFlags: UINT32,
};
pub const ACPI_GTDT_WATCHDOG = struct_acpi_gtdt_watchdog;
pub const struct_acpi_table_hest = extern struct {
    Header: ACPI_TABLE_HEADER,
    ErrorSourceCount: UINT32,
};
pub const ACPI_TABLE_HEST = struct_acpi_table_hest;
pub const struct_acpi_hest_header = extern struct {
    Type: UINT16,
    SourceId: UINT16,
};
pub const ACPI_HEST_HEADER = struct_acpi_hest_header;
pub const ACPI_HEST_TYPE_IA32_CHECK: c_int = 0;
pub const ACPI_HEST_TYPE_IA32_CORRECTED_CHECK: c_int = 1;
pub const ACPI_HEST_TYPE_IA32_NMI: c_int = 2;
pub const ACPI_HEST_TYPE_NOT_USED3: c_int = 3;
pub const ACPI_HEST_TYPE_NOT_USED4: c_int = 4;
pub const ACPI_HEST_TYPE_NOT_USED5: c_int = 5;
pub const ACPI_HEST_TYPE_AER_ROOT_PORT: c_int = 6;
pub const ACPI_HEST_TYPE_AER_ENDPOINT: c_int = 7;
pub const ACPI_HEST_TYPE_AER_BRIDGE: c_int = 8;
pub const ACPI_HEST_TYPE_GENERIC_ERROR: c_int = 9;
pub const ACPI_HEST_TYPE_GENERIC_ERROR_V2: c_int = 10;
pub const ACPI_HEST_TYPE_IA32_DEFERRED_CHECK: c_int = 11;
pub const ACPI_HEST_TYPE_RESERVED: c_int = 12;
pub const enum_AcpiHestTypes = c_uint;
pub const struct_acpi_hest_ia_error_bank = extern struct {
    BankNumber: UINT8,
    ClearStatusOnInit: UINT8,
    StatusFormat: UINT8,
    Reserved: UINT8,
    ControlRegister: UINT32,
    ControlData: UINT64,
    StatusRegister: UINT32,
    AddressRegister: UINT32,
    MiscRegister: UINT32,
};
pub const ACPI_HEST_IA_ERROR_BANK = struct_acpi_hest_ia_error_bank;
pub const struct_acpi_hest_aer_common = extern struct {
    Reserved1: UINT16,
    Flags: UINT8,
    Enabled: UINT8,
    RecordsToPreallocate: UINT32,
    MaxSectionsPerRecord: UINT32,
    Bus: UINT32,
    Device: UINT16,
    Function: UINT16,
    DeviceControl: UINT16,
    Reserved2: UINT16,
    UncorrectableMask: UINT32,
    UncorrectableSeverity: UINT32,
    CorrectableMask: UINT32,
    AdvancedCapabilities: UINT32,
};
pub const ACPI_HEST_AER_COMMON = struct_acpi_hest_aer_common;
pub const struct_acpi_hest_notify = extern struct {
    Type: UINT8,
    Length: UINT8,
    ConfigWriteEnable: UINT16,
    PollInterval: UINT32,
    Vector: UINT32,
    PollingThresholdValue: UINT32,
    PollingThresholdWindow: UINT32,
    ErrorThresholdValue: UINT32,
    ErrorThresholdWindow: UINT32,
};
pub const ACPI_HEST_NOTIFY = struct_acpi_hest_notify;
pub const ACPI_HEST_NOTIFY_POLLED: c_int = 0;
pub const ACPI_HEST_NOTIFY_EXTERNAL: c_int = 1;
pub const ACPI_HEST_NOTIFY_LOCAL: c_int = 2;
pub const ACPI_HEST_NOTIFY_SCI: c_int = 3;
pub const ACPI_HEST_NOTIFY_NMI: c_int = 4;
pub const ACPI_HEST_NOTIFY_CMCI: c_int = 5;
pub const ACPI_HEST_NOTIFY_MCE: c_int = 6;
pub const ACPI_HEST_NOTIFY_GPIO: c_int = 7;
pub const ACPI_HEST_NOTIFY_SEA: c_int = 8;
pub const ACPI_HEST_NOTIFY_SEI: c_int = 9;
pub const ACPI_HEST_NOTIFY_GSIV: c_int = 10;
pub const ACPI_HEST_NOTIFY_SOFTWARE_DELEGATED: c_int = 11;
pub const ACPI_HEST_NOTIFY_RESERVED: c_int = 12;
pub const enum_AcpiHestNotifyTypes = c_uint;
pub const struct_acpi_hest_ia_machine_check = extern struct {
    Header: ACPI_HEST_HEADER,
    Reserved1: UINT16,
    Flags: UINT8,
    Enabled: UINT8,
    RecordsToPreallocate: UINT32,
    MaxSectionsPerRecord: UINT32,
    GlobalCapabilityData: UINT64,
    GlobalControlData: UINT64,
    NumHardwareBanks: UINT8,
    Reserved3: [7]UINT8,
};
pub const ACPI_HEST_IA_MACHINE_CHECK = struct_acpi_hest_ia_machine_check;
pub const struct_acpi_hest_ia_corrected = extern struct {
    Header: ACPI_HEST_HEADER,
    Reserved1: UINT16,
    Flags: UINT8,
    Enabled: UINT8,
    RecordsToPreallocate: UINT32,
    MaxSectionsPerRecord: UINT32,
    Notify: ACPI_HEST_NOTIFY,
    NumHardwareBanks: UINT8,
    Reserved2: [3]UINT8,
};
pub const ACPI_HEST_IA_CORRECTED = struct_acpi_hest_ia_corrected;
pub const struct_acpi_hest_ia_nmi = extern struct {
    Header: ACPI_HEST_HEADER,
    Reserved: UINT32,
    RecordsToPreallocate: UINT32,
    MaxSectionsPerRecord: UINT32,
    MaxRawDataLength: UINT32,
};
pub const ACPI_HEST_IA_NMI = struct_acpi_hest_ia_nmi;
pub const struct_acpi_hest_aer_root = extern struct {
    Header: ACPI_HEST_HEADER,
    Aer: ACPI_HEST_AER_COMMON,
    RootErrorCommand: UINT32,
};
pub const ACPI_HEST_AER_ROOT = struct_acpi_hest_aer_root;
pub const struct_acpi_hest_aer = extern struct {
    Header: ACPI_HEST_HEADER,
    Aer: ACPI_HEST_AER_COMMON,
};
pub const ACPI_HEST_AER = struct_acpi_hest_aer;
pub const struct_acpi_hest_aer_bridge = extern struct {
    Header: ACPI_HEST_HEADER,
    Aer: ACPI_HEST_AER_COMMON,
    UncorrectableMask2: UINT32,
    UncorrectableSeverity2: UINT32,
    AdvancedCapabilities2: UINT32,
};
pub const ACPI_HEST_AER_BRIDGE = struct_acpi_hest_aer_bridge;
pub const struct_acpi_hest_generic = extern struct {
    Header: ACPI_HEST_HEADER,
    RelatedSourceId: UINT16,
    Reserved: UINT8,
    Enabled: UINT8,
    RecordsToPreallocate: UINT32,
    MaxSectionsPerRecord: UINT32,
    MaxRawDataLength: UINT32,
    ErrorStatusAddress: ACPI_GENERIC_ADDRESS,
    Notify: ACPI_HEST_NOTIFY,
    ErrorBlockLength: UINT32,
};
pub const ACPI_HEST_GENERIC = struct_acpi_hest_generic;
pub const struct_acpi_hest_generic_v2 = extern struct {
    Header: ACPI_HEST_HEADER,
    RelatedSourceId: UINT16,
    Reserved: UINT8,
    Enabled: UINT8,
    RecordsToPreallocate: UINT32,
    MaxSectionsPerRecord: UINT32,
    MaxRawDataLength: UINT32,
    ErrorStatusAddress: ACPI_GENERIC_ADDRESS,
    Notify: ACPI_HEST_NOTIFY,
    ErrorBlockLength: UINT32,
    ReadAckRegister: ACPI_GENERIC_ADDRESS,
    ReadAckPreserve: UINT64,
    ReadAckWrite: UINT64,
};
pub const ACPI_HEST_GENERIC_V2 = struct_acpi_hest_generic_v2;
pub const struct_acpi_hest_generic_status = extern struct {
    BlockStatus: UINT32,
    RawDataOffset: UINT32,
    RawDataLength: UINT32,
    DataLength: UINT32,
    ErrorSeverity: UINT32,
};
pub const ACPI_HEST_GENERIC_STATUS = struct_acpi_hest_generic_status;
pub const struct_acpi_hest_generic_data = extern struct {
    SectionType: [16]UINT8,
    ErrorSeverity: UINT32,
    Revision: UINT16,
    ValidationBits: UINT8,
    Flags: UINT8,
    ErrorDataLength: UINT32,
    FruId: [16]UINT8,
    FruText: [20]UINT8,
};
pub const ACPI_HEST_GENERIC_DATA = struct_acpi_hest_generic_data;
pub const struct_acpi_hest_generic_data_v300 = extern struct {
    SectionType: [16]UINT8,
    ErrorSeverity: UINT32,
    Revision: UINT16,
    ValidationBits: UINT8,
    Flags: UINT8,
    ErrorDataLength: UINT32,
    FruId: [16]UINT8,
    FruText: [20]UINT8,
    TimeStamp: UINT64,
};
pub const ACPI_HEST_GENERIC_DATA_V300 = struct_acpi_hest_generic_data_v300;
pub const struct_acpi_hest_ia_deferred_check = extern struct {
    Header: ACPI_HEST_HEADER,
    Reserved1: UINT16,
    Flags: UINT8,
    Enabled: UINT8,
    RecordsToPreallocate: UINT32,
    MaxSectionsPerRecord: UINT32,
    Notify: ACPI_HEST_NOTIFY,
    NumHardwareBanks: UINT8,
    Reserved2: [3]UINT8,
};
pub const ACPI_HEST_IA_DEFERRED_CHECK = struct_acpi_hest_ia_deferred_check;
pub const struct_acpi_table_hmat = extern struct {
    Header: ACPI_TABLE_HEADER,
    Reserved: UINT32,
};
pub const ACPI_TABLE_HMAT = struct_acpi_table_hmat;
pub const ACPI_HMAT_TYPE_ADDRESS_RANGE: c_int = 0;
pub const ACPI_HMAT_TYPE_LOCALITY: c_int = 1;
pub const ACPI_HMAT_TYPE_CACHE: c_int = 2;
pub const ACPI_HMAT_TYPE_RESERVED: c_int = 3;
pub const enum_AcpiHmatType = c_uint;
pub const struct_acpi_hmat_structure = extern struct {
    Type: UINT16,
    Reserved: UINT16,
    Length: UINT32,
};
pub const ACPI_HMAT_STRUCTURE = struct_acpi_hmat_structure;
pub const struct_acpi_hmat_proximity_domain = extern struct {
    Header: ACPI_HMAT_STRUCTURE,
    Flags: UINT16,
    Reserved1: UINT16,
    InitiatorPD: UINT32,
    MemoryPD: UINT32,
    Reserved2: UINT32,
    Reserved3: UINT64,
    Reserved4: UINT64,
};
pub const ACPI_HMAT_PROXIMITY_DOMAIN = struct_acpi_hmat_proximity_domain;
pub const struct_acpi_hmat_locality = extern struct {
    Header: ACPI_HMAT_STRUCTURE,
    Flags: UINT8,
    DataType: UINT8,
    MinTransferSize: UINT8,
    Reserved1: UINT8,
    NumberOfInitiatorPDs: UINT32,
    NumberOfTargetPDs: UINT32,
    Reserved2: UINT32,
    EntryBaseUnit: UINT64,
};
pub const ACPI_HMAT_LOCALITY = struct_acpi_hmat_locality;
pub const struct_acpi_hmat_cache = extern struct {
    Header: ACPI_HMAT_STRUCTURE,
    MemoryPD: UINT32,
    Reserved1: UINT32,
    CacheSize: UINT64,
    CacheAttributes: UINT32,
    Reserved2: UINT16,
    NumberOfSMBIOSHandles: UINT16,
};
pub const ACPI_HMAT_CACHE = struct_acpi_hmat_cache;
pub const struct_acpi_table_hpet = extern struct {
    Header: ACPI_TABLE_HEADER,
    Id: UINT32,
    Address: ACPI_GENERIC_ADDRESS,
    Sequence: UINT8,
    MinimumTick: UINT16,
    Flags: UINT8,
};
pub const ACPI_TABLE_HPET = struct_acpi_table_hpet;
pub const ACPI_HPET_NO_PAGE_PROTECT: c_int = 0;
pub const ACPI_HPET_PAGE_PROTECT4: c_int = 1;
pub const ACPI_HPET_PAGE_PROTECT64: c_int = 2;
pub const enum_AcpiHpetPageProtect = c_uint;
pub const struct_acpi_table_ibft = extern struct {
    Header: ACPI_TABLE_HEADER,
    Reserved: [12]UINT8,
};
pub const ACPI_TABLE_IBFT = struct_acpi_table_ibft;
pub const struct_acpi_ibft_header = extern struct {
    Type: UINT8,
    Version: UINT8,
    Length: UINT16,
    Index: UINT8,
    Flags: UINT8,
};
pub const ACPI_IBFT_HEADER = struct_acpi_ibft_header;
pub const ACPI_IBFT_TYPE_NOT_USED: c_int = 0;
pub const ACPI_IBFT_TYPE_CONTROL: c_int = 1;
pub const ACPI_IBFT_TYPE_INITIATOR: c_int = 2;
pub const ACPI_IBFT_TYPE_NIC: c_int = 3;
pub const ACPI_IBFT_TYPE_TARGET: c_int = 4;
pub const ACPI_IBFT_TYPE_EXTENSIONS: c_int = 5;
pub const ACPI_IBFT_TYPE_RESERVED: c_int = 6;
pub const enum_AcpiIbftType = c_uint;
pub const struct_acpi_ibft_control = extern struct {
    Header: ACPI_IBFT_HEADER,
    Extensions: UINT16,
    InitiatorOffset: UINT16,
    Nic0Offset: UINT16,
    Target0Offset: UINT16,
    Nic1Offset: UINT16,
    Target1Offset: UINT16,
};
pub const ACPI_IBFT_CONTROL = struct_acpi_ibft_control;
pub const struct_acpi_ibft_initiator = extern struct {
    Header: ACPI_IBFT_HEADER,
    SnsServer: [16]UINT8,
    SlpServer: [16]UINT8,
    PrimaryServer: [16]UINT8,
    SecondaryServer: [16]UINT8,
    NameLength: UINT16,
    NameOffset: UINT16,
};
pub const ACPI_IBFT_INITIATOR = struct_acpi_ibft_initiator;
pub const struct_acpi_ibft_nic = extern struct {
    Header: ACPI_IBFT_HEADER,
    IpAddress: [16]UINT8,
    SubnetMaskPrefix: UINT8,
    Origin: UINT8,
    Gateway: [16]UINT8,
    PrimaryDns: [16]UINT8,
    SecondaryDns: [16]UINT8,
    Dhcp: [16]UINT8,
    Vlan: UINT16,
    MacAddress: [6]UINT8,
    PciAddress: UINT16,
    NameLength: UINT16,
    NameOffset: UINT16,
};
pub const ACPI_IBFT_NIC = struct_acpi_ibft_nic;
pub const struct_acpi_ibft_target = extern struct {
    Header: ACPI_IBFT_HEADER,
    TargetIpAddress: [16]UINT8,
    TargetIpSocket: UINT16,
    TargetBootLun: [8]UINT8,
    ChapType: UINT8,
    NicAssociation: UINT8,
    TargetNameLength: UINT16,
    TargetNameOffset: UINT16,
    ChapNameLength: UINT16,
    ChapNameOffset: UINT16,
    ChapSecretLength: UINT16,
    ChapSecretOffset: UINT16,
    ReverseChapNameLength: UINT16,
    ReverseChapNameOffset: UINT16,
    ReverseChapSecretLength: UINT16,
    ReverseChapSecretOffset: UINT16,
};
pub const ACPI_IBFT_TARGET = struct_acpi_ibft_target;
pub const struct_acpi_table_aest = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_AEST = struct_acpi_table_aest;
pub const struct_acpi_aest_hdr = extern struct {
    Type: UINT8,
    Length: UINT16,
    Reserved: UINT8,
    NodeSpecificOffset: UINT32,
    NodeInterfaceOffset: UINT32,
    NodeInterruptOffset: UINT32,
    NodeInterruptCount: UINT32,
    TimestampRate: UINT64,
    Reserved1: UINT64,
    ErrorInjectionRate: UINT64,
};
pub const ACPI_AEST_HEADER = struct_acpi_aest_hdr;
pub const struct_acpi_aest_processor = extern struct {
    ProcessorId: UINT32,
    ResourceType: UINT8,
    Reserved: UINT8,
    Flags: UINT8,
    Revision: UINT8,
    ProcessorAffinity: UINT64,
};
pub const ACPI_AEST_PROCESSOR = struct_acpi_aest_processor;
pub const struct_acpi_aest_processor_cache = extern struct {
    CacheReference: UINT32,
    Reserved: UINT32,
};
pub const ACPI_AEST_PROCESSOR_CACHE = struct_acpi_aest_processor_cache;
pub const struct_acpi_aest_processor_tlb = extern struct {
    TlbLevel: UINT32,
    Reserved: UINT32,
};
pub const ACPI_AEST_PROCESSOR_TLB = struct_acpi_aest_processor_tlb;
pub const struct_acpi_aest_processor_generic = extern struct {
    Resource: UINT32,
};
pub const ACPI_AEST_PROCESSOR_GENERIC = struct_acpi_aest_processor_generic;
pub const struct_acpi_aest_memory = extern struct {
    SratProximityDomain: UINT32,
};
pub const ACPI_AEST_MEMORY = struct_acpi_aest_memory;
pub const struct_acpi_aest_smmu = extern struct {
    IortNodeReference: UINT32,
    SubcomponentReference: UINT32,
};
pub const ACPI_AEST_SMMU = struct_acpi_aest_smmu;
pub const struct_acpi_aest_vendor = extern struct {
    AcpiHid: UINT32,
    AcpiUid: UINT32,
    VendorSpecificData: [16]UINT8,
};
pub const ACPI_AEST_VENDOR = struct_acpi_aest_vendor;
pub const struct_acpi_aest_gic = extern struct {
    InterfaceType: UINT32,
    InstanceId: UINT32,
};
pub const ACPI_AEST_GIC = struct_acpi_aest_gic;
pub const struct_acpi_aest_node_interface = extern struct {
    Type: UINT8,
    Reserved: [3]UINT8,
    Flags: UINT32,
    Address: UINT64,
    ErrorRecordIndex: UINT32,
    ErrorRecordCount: UINT32,
    ErrorRecordImplemented: UINT64,
    ErrorStatusReporting: UINT64,
    AddressingMode: UINT64,
};
pub const ACPI_AEST_NODE_INTERFACE = struct_acpi_aest_node_interface;
pub const struct_acpi_aest_node_interrupt = extern struct {
    Type: UINT8,
    Reserved: [2]UINT8,
    Flags: UINT8,
    Gsiv: UINT32,
    IortId: UINT8,
    Reserved1: [3]UINT8,
};
pub const ACPI_AEST_NODE_INTERRUPT = struct_acpi_aest_node_interrupt;
pub const struct_acpi_table_agdi = extern struct {
    Header: ACPI_TABLE_HEADER,
    Flags: UINT8,
    Reserved: [3]UINT8,
    SdeiEvent: UINT32,
    Gsiv: UINT32,
};
pub const ACPI_TABLE_AGDI = struct_acpi_table_agdi;
pub const struct_acpi_table_apmt = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_APMT = struct_acpi_table_apmt;
pub const struct_acpi_apmt_node = extern struct {
    Length: UINT16,
    Flags: UINT8,
    Type: UINT8,
    Id: UINT32,
    InstPrimary: UINT64,
    InstSecondary: UINT32,
    BaseAddress0: UINT64,
    BaseAddress1: UINT64,
    OvflwIrq: UINT32,
    Reserved: UINT32,
    OvflwIrqFlags: UINT32,
    ProcAffinity: UINT32,
    ImplId: UINT32,
};
pub const ACPI_APMT_NODE = struct_acpi_apmt_node;
pub const ACPI_APMT_NODE_TYPE_MC: c_int = 0;
pub const ACPI_APMT_NODE_TYPE_SMMU: c_int = 1;
pub const ACPI_APMT_NODE_TYPE_PCIE_ROOT: c_int = 2;
pub const ACPI_APMT_NODE_TYPE_ACPI: c_int = 3;
pub const ACPI_APMT_NODE_TYPE_CACHE: c_int = 4;
pub const ACPI_APMT_NODE_TYPE_COUNT: c_int = 5;
pub const enum_acpi_apmt_node_type = c_uint;
pub const struct_acpi_table_bdat = extern struct {
    Header: ACPI_TABLE_HEADER,
    Gas: ACPI_GENERIC_ADDRESS,
};
pub const ACPI_TABLE_BDAT = struct_acpi_table_bdat;
pub const struct_acpi_table_ccel = extern struct {
    Header: ACPI_TABLE_HEADER,
    CCType: UINT8,
    CCSubType: UINT8,
    Reserved: UINT16,
    LogAreaMinimumLength: UINT64,
    LogAreaStartAddress: UINT64,
};
pub const ACPI_TABLE_CCEL = struct_acpi_table_ccel;
pub const struct_acpi_table_iort = extern struct {
    Header: ACPI_TABLE_HEADER,
    NodeCount: UINT32,
    NodeOffset: UINT32,
    Reserved: UINT32,
};
pub const ACPI_TABLE_IORT = struct_acpi_table_iort;
pub const struct_acpi_iort_node = extern struct {
    Type: UINT8 align(1),
    Length: UINT16,
    Revision: UINT8,
    Identifier: UINT32,
    MappingCount: UINT32,
    MappingOffset: UINT32,
    pub fn NodeData(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 16)));
    }
};
pub const ACPI_IORT_NODE = struct_acpi_iort_node;
pub const ACPI_IORT_NODE_ITS_GROUP: c_int = 0;
pub const ACPI_IORT_NODE_NAMED_COMPONENT: c_int = 1;
pub const ACPI_IORT_NODE_PCI_ROOT_COMPLEX: c_int = 2;
pub const ACPI_IORT_NODE_SMMU: c_int = 3;
pub const ACPI_IORT_NODE_SMMU_V3: c_int = 4;
pub const ACPI_IORT_NODE_PMCG: c_int = 5;
pub const ACPI_IORT_NODE_RMR: c_int = 6;
pub const enum_AcpiIortNodeType = c_uint;
pub const struct_acpi_iort_id_mapping = extern struct {
    InputBase: UINT32,
    IdCount: UINT32,
    OutputBase: UINT32,
    OutputReference: UINT32,
    Flags: UINT32,
};
pub const ACPI_IORT_ID_MAPPING = struct_acpi_iort_id_mapping;
pub const struct_acpi_iort_memory_access = extern struct {
    CacheCoherency: UINT32,
    Hints: UINT8,
    Reserved: UINT16,
    MemoryFlags: UINT8,
};
pub const ACPI_IORT_MEMORY_ACCESS = struct_acpi_iort_memory_access;
pub const struct_acpi_iort_its_group = extern struct {
    ItsCount: UINT32 align(1),
    pub fn Identifiers(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT32) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT32);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 4)));
    }
};
pub const ACPI_IORT_ITS_GROUP = struct_acpi_iort_its_group;
pub const struct_acpi_iort_named_component = extern struct {
    NodeFlags: UINT32 align(1),
    MemoryProperties: UINT64,
    MemoryAddressLimit: UINT8,
    pub fn DeviceName(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 13)));
    }
};
pub const ACPI_IORT_NAMED_COMPONENT = struct_acpi_iort_named_component;
pub const struct_acpi_iort_root_complex = extern struct {
    MemoryProperties: UINT64 align(1),
    AtsAttribute: UINT32,
    PciSegmentNumber: UINT32,
    MemoryAddressLimit: UINT8,
    PasidCapabilities: UINT16,
    pub fn Reserved(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 19)));
    }
};
pub const ACPI_IORT_ROOT_COMPLEX = struct_acpi_iort_root_complex;
pub const struct_acpi_iort_smmu = extern struct {
    BaseAddress: UINT64 align(1),
    Span: UINT64,
    Model: UINT32,
    Flags: UINT32,
    GlobalInterruptOffset: UINT32,
    ContextInterruptCount: UINT32,
    ContextInterruptOffset: UINT32,
    PmuInterruptCount: UINT32,
    PmuInterruptOffset: UINT32,
    pub fn Interrupts(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT64) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT64);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 44)));
    }
};
pub const ACPI_IORT_SMMU = struct_acpi_iort_smmu;
pub const struct_acpi_iort_smmu_gsi = extern struct {
    NSgIrpt: UINT32,
    NSgIrptFlags: UINT32,
    NSgCfgIrpt: UINT32,
    NSgCfgIrptFlags: UINT32,
};
pub const ACPI_IORT_SMMU_GSI = struct_acpi_iort_smmu_gsi;
pub const struct_acpi_iort_smmu_v3 = extern struct {
    BaseAddress: UINT64,
    Flags: UINT32,
    Reserved: UINT32,
    VatosAddress: UINT64,
    Model: UINT32,
    EventGsiv: UINT32,
    PriGsiv: UINT32,
    GerrGsiv: UINT32,
    SyncGsiv: UINT32,
    Pxm: UINT32,
    IdMappingIndex: UINT32,
};
pub const ACPI_IORT_SMMU_V3 = struct_acpi_iort_smmu_v3;
pub const struct_acpi_iort_pmcg = extern struct {
    Page0BaseAddress: UINT64,
    OverflowGsiv: UINT32,
    NodeReference: UINT32,
    Page1BaseAddress: UINT64,
};
pub const ACPI_IORT_PMCG = struct_acpi_iort_pmcg;
pub const struct_acpi_iort_rmr = extern struct {
    Flags: UINT32,
    RmrCount: UINT32,
    RmrOffset: UINT32,
};
pub const ACPI_IORT_RMR = struct_acpi_iort_rmr;
pub const struct_acpi_iort_rmr_desc = extern struct {
    BaseAddress: UINT64,
    Length: UINT64,
    Reserved: UINT32,
};
pub const ACPI_IORT_RMR_DESC = struct_acpi_iort_rmr_desc;
pub const struct_acpi_table_ivrs = extern struct {
    Header: ACPI_TABLE_HEADER,
    Info: UINT32,
    Reserved: UINT64,
};
pub const ACPI_TABLE_IVRS = struct_acpi_table_ivrs;
pub const struct_acpi_ivrs_header = extern struct {
    Type: UINT8,
    Flags: UINT8,
    Length: UINT16,
    DeviceId: UINT16,
};
pub const ACPI_IVRS_HEADER = struct_acpi_ivrs_header;
pub const ACPI_IVRS_TYPE_HARDWARE1: c_int = 16;
pub const ACPI_IVRS_TYPE_HARDWARE2: c_int = 17;
pub const ACPI_IVRS_TYPE_HARDWARE3: c_int = 64;
pub const ACPI_IVRS_TYPE_MEMORY1: c_int = 32;
pub const ACPI_IVRS_TYPE_MEMORY2: c_int = 33;
pub const ACPI_IVRS_TYPE_MEMORY3: c_int = 34;
pub const enum_AcpiIvrsType = c_uint;
pub const struct_acpi_ivrs_hardware_10 = extern struct {
    Header: ACPI_IVRS_HEADER,
    CapabilityOffset: UINT16,
    BaseAddress: UINT64,
    PciSegmentGroup: UINT16,
    Info: UINT16,
    FeatureReporting: UINT32,
};
pub const ACPI_IVRS_HARDWARE1 = struct_acpi_ivrs_hardware_10;
pub const struct_acpi_ivrs_hardware_11 = extern struct {
    Header: ACPI_IVRS_HEADER,
    CapabilityOffset: UINT16,
    BaseAddress: UINT64,
    PciSegmentGroup: UINT16,
    Info: UINT16,
    Attributes: UINT32,
    EfrRegisterImage: UINT64,
    Reserved: UINT64,
};
pub const ACPI_IVRS_HARDWARE2 = struct_acpi_ivrs_hardware_11;
pub const struct_acpi_ivrs_de_header = extern struct {
    Type: UINT8,
    Id: UINT16,
    DataSetting: UINT8,
};
pub const ACPI_IVRS_DE_HEADER = struct_acpi_ivrs_de_header;
pub const ACPI_IVRS_TYPE_PAD4: c_int = 0;
pub const ACPI_IVRS_TYPE_ALL: c_int = 1;
pub const ACPI_IVRS_TYPE_SELECT: c_int = 2;
pub const ACPI_IVRS_TYPE_START: c_int = 3;
pub const ACPI_IVRS_TYPE_END: c_int = 4;
pub const ACPI_IVRS_TYPE_PAD8: c_int = 64;
pub const ACPI_IVRS_TYPE_NOT_USED: c_int = 65;
pub const ACPI_IVRS_TYPE_ALIAS_SELECT: c_int = 66;
pub const ACPI_IVRS_TYPE_ALIAS_START: c_int = 67;
pub const ACPI_IVRS_TYPE_EXT_SELECT: c_int = 70;
pub const ACPI_IVRS_TYPE_EXT_START: c_int = 71;
pub const ACPI_IVRS_TYPE_SPECIAL: c_int = 72;
pub const ACPI_IVRS_TYPE_HID: c_int = 240;
pub const enum_AcpiIvrsDeviceEntryType = c_uint;
pub const struct_acpi_ivrs_device4 = extern struct {
    Header: ACPI_IVRS_DE_HEADER,
};
pub const ACPI_IVRS_DEVICE4 = struct_acpi_ivrs_device4;
pub const struct_acpi_ivrs_device8a = extern struct {
    Header: ACPI_IVRS_DE_HEADER,
    Reserved1: UINT8,
    UsedId: UINT16,
    Reserved2: UINT8,
};
pub const ACPI_IVRS_DEVICE8A = struct_acpi_ivrs_device8a;
pub const struct_acpi_ivrs_device8b = extern struct {
    Header: ACPI_IVRS_DE_HEADER,
    ExtendedData: UINT32,
};
pub const ACPI_IVRS_DEVICE8B = struct_acpi_ivrs_device8b;
pub const struct_acpi_ivrs_device8c = extern struct {
    Header: ACPI_IVRS_DE_HEADER,
    Handle: UINT8,
    UsedId: UINT16,
    Variety: UINT8,
};
pub const ACPI_IVRS_DEVICE8C = struct_acpi_ivrs_device8c;
pub const struct_acpi_ivrs_device_hid = extern struct {
    Header: ACPI_IVRS_DE_HEADER,
    AcpiHid: UINT64,
    AcpiCid: UINT64,
    UidType: UINT8,
    UidLength: UINT8,
};
pub const ACPI_IVRS_DEVICE_HID = struct_acpi_ivrs_device_hid;
pub const struct_acpi_ivrs_memory = extern struct {
    Header: ACPI_IVRS_HEADER,
    AuxData: UINT16,
    Reserved: UINT64,
    StartAddress: UINT64,
    MemoryLength: UINT64,
};
pub const ACPI_IVRS_MEMORY = struct_acpi_ivrs_memory;
pub const struct_acpi_table_lpit = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_LPIT = struct_acpi_table_lpit;
pub const struct_acpi_lpit_header = extern struct {
    Type: UINT32,
    Length: UINT32,
    UniqueId: UINT16,
    Reserved: UINT16,
    Flags: UINT32,
};
pub const ACPI_LPIT_HEADER = struct_acpi_lpit_header;
pub const ACPI_LPIT_TYPE_NATIVE_CSTATE: c_int = 0;
pub const ACPI_LPIT_TYPE_RESERVED: c_int = 1;
pub const enum_AcpiLpitType = c_uint;
pub const struct_acpi_lpit_native = extern struct {
    Header: ACPI_LPIT_HEADER,
    EntryTrigger: ACPI_GENERIC_ADDRESS,
    Residency: UINT32,
    Latency: UINT32,
    ResidencyCounter: ACPI_GENERIC_ADDRESS,
    CounterFrequency: UINT64,
};
pub const ACPI_LPIT_NATIVE = struct_acpi_lpit_native;
pub const struct_acpi_table_madt = extern struct {
    Header: ACPI_TABLE_HEADER,
    Address: UINT32,
    Flags: UINT32,
};
pub const ACPI_TABLE_MADT = struct_acpi_table_madt;
pub const ACPI_MADT_TYPE_LOCAL_APIC: c_int = 0;
pub const ACPI_MADT_TYPE_IO_APIC: c_int = 1;
pub const ACPI_MADT_TYPE_INTERRUPT_OVERRIDE: c_int = 2;
pub const ACPI_MADT_TYPE_NMI_SOURCE: c_int = 3;
pub const ACPI_MADT_TYPE_LOCAL_APIC_NMI: c_int = 4;
pub const ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE: c_int = 5;
pub const ACPI_MADT_TYPE_IO_SAPIC: c_int = 6;
pub const ACPI_MADT_TYPE_LOCAL_SAPIC: c_int = 7;
pub const ACPI_MADT_TYPE_INTERRUPT_SOURCE: c_int = 8;
pub const ACPI_MADT_TYPE_LOCAL_X2APIC: c_int = 9;
pub const ACPI_MADT_TYPE_LOCAL_X2APIC_NMI: c_int = 10;
pub const ACPI_MADT_TYPE_GENERIC_INTERRUPT: c_int = 11;
pub const ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR: c_int = 12;
pub const ACPI_MADT_TYPE_GENERIC_MSI_FRAME: c_int = 13;
pub const ACPI_MADT_TYPE_GENERIC_REDISTRIBUTOR: c_int = 14;
pub const ACPI_MADT_TYPE_GENERIC_TRANSLATOR: c_int = 15;
pub const ACPI_MADT_TYPE_MULTIPROC_WAKEUP: c_int = 16;
pub const ACPI_MADT_TYPE_CORE_PIC: c_int = 17;
pub const ACPI_MADT_TYPE_LIO_PIC: c_int = 18;
pub const ACPI_MADT_TYPE_HT_PIC: c_int = 19;
pub const ACPI_MADT_TYPE_EIO_PIC: c_int = 20;
pub const ACPI_MADT_TYPE_MSI_PIC: c_int = 21;
pub const ACPI_MADT_TYPE_BIO_PIC: c_int = 22;
pub const ACPI_MADT_TYPE_LPC_PIC: c_int = 23;
pub const ACPI_MADT_TYPE_RINTC: c_int = 24;
pub const ACPI_MADT_TYPE_IMSIC: c_int = 25;
pub const ACPI_MADT_TYPE_APLIC: c_int = 26;
pub const ACPI_MADT_TYPE_PLIC: c_int = 27;
pub const ACPI_MADT_TYPE_RESERVED: c_int = 28;
pub const ACPI_MADT_TYPE_OEM_RESERVED: c_int = 128;
pub const enum_AcpiMadtType = c_uint;
pub const struct_acpi_madt_local_apic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    ProcessorId: UINT8,
    Id: UINT8,
    LapicFlags: UINT32,
};
pub const ACPI_MADT_LOCAL_APIC = struct_acpi_madt_local_apic;
pub const struct_acpi_madt_io_apic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Id: UINT8,
    Reserved: UINT8,
    Address: UINT32,
    GlobalIrqBase: UINT32,
};
pub const ACPI_MADT_IO_APIC = struct_acpi_madt_io_apic;
pub const struct_acpi_madt_interrupt_override = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Bus: UINT8,
    SourceIrq: UINT8,
    GlobalIrq: UINT32,
    IntiFlags: UINT16,
};
pub const ACPI_MADT_INTERRUPT_OVERRIDE = struct_acpi_madt_interrupt_override;
pub const struct_acpi_madt_nmi_source = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    IntiFlags: UINT16,
    GlobalIrq: UINT32,
};
pub const ACPI_MADT_NMI_SOURCE = struct_acpi_madt_nmi_source;
pub const struct_acpi_madt_local_apic_nmi = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    ProcessorId: UINT8,
    IntiFlags: UINT16,
    Lint: UINT8,
};
pub const ACPI_MADT_LOCAL_APIC_NMI = struct_acpi_madt_local_apic_nmi;
pub const struct_acpi_madt_local_apic_override = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Reserved: UINT16,
    Address: UINT64,
};
pub const ACPI_MADT_LOCAL_APIC_OVERRIDE = struct_acpi_madt_local_apic_override;
pub const struct_acpi_madt_io_sapic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Id: UINT8,
    Reserved: UINT8,
    GlobalIrqBase: UINT32,
    Address: UINT64,
};
pub const ACPI_MADT_IO_SAPIC = struct_acpi_madt_io_sapic;
pub const struct_acpi_madt_local_sapic = extern struct {
    Header: ACPI_SUBTABLE_HEADER align(1),
    ProcessorId: UINT8,
    Id: UINT8,
    Eid: UINT8,
    Reserved: [3]UINT8,
    LapicFlags: UINT32,
    Uid: UINT32,
    pub fn UidString(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 16)));
    }
};
pub const ACPI_MADT_LOCAL_SAPIC = struct_acpi_madt_local_sapic;
pub const struct_acpi_madt_interrupt_source = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    IntiFlags: UINT16,
    Type: UINT8,
    Id: UINT8,
    Eid: UINT8,
    IoSapicVector: UINT8,
    GlobalIrq: UINT32,
    Flags: UINT32,
};
pub const ACPI_MADT_INTERRUPT_SOURCE = struct_acpi_madt_interrupt_source;
pub const struct_acpi_madt_local_x2apic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Reserved: UINT16,
    LocalApicId: UINT32,
    LapicFlags: UINT32,
    Uid: UINT32,
};
pub const ACPI_MADT_LOCAL_X2APIC = struct_acpi_madt_local_x2apic;
pub const struct_acpi_madt_local_x2apic_nmi = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    IntiFlags: UINT16,
    Uid: UINT32,
    Lint: UINT8,
    Reserved: [3]UINT8,
};
pub const ACPI_MADT_LOCAL_X2APIC_NMI = struct_acpi_madt_local_x2apic_nmi;
pub const struct_acpi_madt_generic_interrupt = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Reserved: UINT16,
    CpuInterfaceNumber: UINT32,
    Uid: UINT32,
    Flags: UINT32,
    ParkingVersion: UINT32,
    PerformanceInterrupt: UINT32,
    ParkedAddress: UINT64,
    BaseAddress: UINT64,
    GicvBaseAddress: UINT64,
    GichBaseAddress: UINT64,
    VgicInterrupt: UINT32,
    GicrBaseAddress: UINT64,
    ArmMpidr: UINT64,
    EfficiencyClass: UINT8,
    Reserved2: [1]UINT8,
    SpeInterrupt: UINT16,
    TrbeInterrupt: UINT16,
};
pub const ACPI_MADT_GENERIC_INTERRUPT = struct_acpi_madt_generic_interrupt;
pub const struct_acpi_madt_generic_distributor = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Reserved: UINT16,
    GicId: UINT32,
    BaseAddress: UINT64,
    GlobalIrqBase: UINT32,
    Version: UINT8,
    Reserved2: [3]UINT8,
};
pub const ACPI_MADT_GENERIC_DISTRIBUTOR = struct_acpi_madt_generic_distributor;
pub const ACPI_MADT_GIC_VERSION_NONE: c_int = 0;
pub const ACPI_MADT_GIC_VERSION_V1: c_int = 1;
pub const ACPI_MADT_GIC_VERSION_V2: c_int = 2;
pub const ACPI_MADT_GIC_VERSION_V3: c_int = 3;
pub const ACPI_MADT_GIC_VERSION_V4: c_int = 4;
pub const ACPI_MADT_GIC_VERSION_RESERVED: c_int = 5;
pub const enum_AcpiMadtGicVersion = c_uint;
pub const struct_acpi_madt_generic_msi_frame = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Reserved: UINT16,
    MsiFrameId: UINT32,
    BaseAddress: UINT64,
    Flags: UINT32,
    SpiCount: UINT16,
    SpiBase: UINT16,
};
pub const ACPI_MADT_GENERIC_MSI_FRAME = struct_acpi_madt_generic_msi_frame;
pub const struct_acpi_madt_generic_redistributor = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Flags: UINT8,
    Reserved: UINT8,
    BaseAddress: UINT64,
    Length: UINT32,
};
pub const ACPI_MADT_GENERIC_REDISTRIBUTOR = struct_acpi_madt_generic_redistributor;
pub const struct_acpi_madt_generic_translator = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Flags: UINT8,
    Reserved: UINT8,
    TranslationId: UINT32,
    BaseAddress: UINT64,
    Reserved2: UINT32,
};
pub const ACPI_MADT_GENERIC_TRANSLATOR = struct_acpi_madt_generic_translator;
pub const struct_acpi_madt_multiproc_wakeup = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    MailboxVersion: UINT16,
    Reserved: UINT32,
    BaseAddress: UINT64,
};
pub const ACPI_MADT_MULTIPROC_WAKEUP = struct_acpi_madt_multiproc_wakeup;
pub const struct_acpi_madt_multiproc_wakeup_mailbox = extern struct {
    Command: UINT16,
    Reserved: UINT16,
    ApicId: UINT32,
    WakeupVector: UINT64,
    ReservedOs: [2032]UINT8,
    ReservedFirmware: [2048]UINT8,
};
pub const ACPI_MADT_MULTIPROC_WAKEUP_MAILBOX = struct_acpi_madt_multiproc_wakeup_mailbox;
pub const struct_acpi_madt_core_pic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT8,
    ProcessorId: UINT32,
    CoreId: UINT32,
    Flags: UINT32,
};
pub const ACPI_MADT_CORE_PIC = struct_acpi_madt_core_pic;
pub const ACPI_MADT_CORE_PIC_VERSION_NONE: c_int = 0;
pub const ACPI_MADT_CORE_PIC_VERSION_V1: c_int = 1;
pub const ACPI_MADT_CORE_PIC_VERSION_RESERVED: c_int = 2;
pub const enum_AcpiMadtCorePicVersion = c_uint;
pub const struct_acpi_madt_lio_pic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT8,
    Address: UINT64,
    Size: UINT16,
    Cascade: [2]UINT8,
    CascadeMap: [2]UINT32,
};
pub const ACPI_MADT_LIO_PIC = struct_acpi_madt_lio_pic;
pub const ACPI_MADT_LIO_PIC_VERSION_NONE: c_int = 0;
pub const ACPI_MADT_LIO_PIC_VERSION_V1: c_int = 1;
pub const ACPI_MADT_LIO_PIC_VERSION_RESERVED: c_int = 2;
pub const enum_AcpiMadtLioPicVersion = c_uint;
pub const struct_acpi_madt_ht_pic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT8,
    Address: UINT64,
    Size: UINT16,
    Cascade: [8]UINT8,
};
pub const ACPI_MADT_HT_PIC = struct_acpi_madt_ht_pic;
pub const ACPI_MADT_HT_PIC_VERSION_NONE: c_int = 0;
pub const ACPI_MADT_HT_PIC_VERSION_V1: c_int = 1;
pub const ACPI_MADT_HT_PIC_VERSION_RESERVED: c_int = 2;
pub const enum_AcpiMadtHtPicVersion = c_uint;
pub const struct_acpi_madt_eio_pic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT8,
    Cascade: UINT8,
    Node: UINT8,
    NodeMap: UINT64,
};
pub const ACPI_MADT_EIO_PIC = struct_acpi_madt_eio_pic;
pub const ACPI_MADT_EIO_PIC_VERSION_NONE: c_int = 0;
pub const ACPI_MADT_EIO_PIC_VERSION_V1: c_int = 1;
pub const ACPI_MADT_EIO_PIC_VERSION_RESERVED: c_int = 2;
pub const enum_AcpiMadtEioPicVersion = c_uint;
pub const struct_acpi_madt_msi_pic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT8,
    MsgAddress: UINT64,
    Start: UINT32,
    Count: UINT32,
};
pub const ACPI_MADT_MSI_PIC = struct_acpi_madt_msi_pic;
pub const ACPI_MADT_MSI_PIC_VERSION_NONE: c_int = 0;
pub const ACPI_MADT_MSI_PIC_VERSION_V1: c_int = 1;
pub const ACPI_MADT_MSI_PIC_VERSION_RESERVED: c_int = 2;
pub const enum_AcpiMadtMsiPicVersion = c_uint;
pub const struct_acpi_madt_bio_pic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT8,
    Address: UINT64,
    Size: UINT16,
    Id: UINT16,
    GsiBase: UINT16,
};
pub const ACPI_MADT_BIO_PIC = struct_acpi_madt_bio_pic;
pub const ACPI_MADT_BIO_PIC_VERSION_NONE: c_int = 0;
pub const ACPI_MADT_BIO_PIC_VERSION_V1: c_int = 1;
pub const ACPI_MADT_BIO_PIC_VERSION_RESERVED: c_int = 2;
pub const enum_AcpiMadtBioPicVersion = c_uint;
pub const struct_acpi_madt_lpc_pic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT8,
    Address: UINT64,
    Size: UINT16,
    Cascade: UINT8,
};
pub const ACPI_MADT_LPC_PIC = struct_acpi_madt_lpc_pic;
pub const ACPI_MADT_LPC_PIC_VERSION_NONE: c_int = 0;
pub const ACPI_MADT_LPC_PIC_VERSION_V1: c_int = 1;
pub const ACPI_MADT_LPC_PIC_VERSION_RESERVED: c_int = 2;
pub const enum_AcpiMadtLpcPicVersion = c_uint;
pub const struct_acpi_madt_rintc = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT8,
    Reserved: UINT8,
    Flags: UINT32,
    HartId: UINT64,
    Uid: UINT32,
    ExtIntcId: UINT32,
    ImsicAddr: UINT64,
    ImsicSize: UINT32,
};
pub const ACPI_MADT_RINTC = struct_acpi_madt_rintc;
pub const ACPI_MADT_RINTC_VERSION_NONE: c_int = 0;
pub const ACPI_MADT_RINTC_VERSION_V1: c_int = 1;
pub const ACPI_MADT_RINTC_VERSION_RESERVED: c_int = 2;
pub const enum_AcpiMadtRintcVersion = c_uint;
pub const struct_acpi_madt_imsic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT8,
    Reserved: UINT8,
    Flags: UINT32,
    NumIds: UINT16,
    NumGuestIds: UINT16,
    GuestIndexBits: UINT8,
    HartIndexBits: UINT8,
    GroupIndexBits: UINT8,
    GroupIndexShift: UINT8,
};
pub const ACPI_MADT_IMSIC = struct_acpi_madt_imsic;
pub const struct_acpi_madt_aplic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT8,
    Id: UINT8,
    Flags: UINT32,
    HwId: [8]UINT8,
    NumIdcs: UINT16,
    NumSources: UINT16,
    GsiBase: UINT32,
    BaseAddr: UINT64,
    Size: UINT32,
};
pub const ACPI_MADT_APLIC = struct_acpi_madt_aplic;
pub const struct_acpi_madt_plic = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT8,
    Id: UINT8,
    HwId: [8]UINT8,
    NumIrqs: UINT16,
    MaxPrio: UINT16,
    Flags: UINT32,
    Size: UINT32,
    BaseAddr: UINT64,
    GsiBase: UINT32,
};
pub const ACPI_MADT_PLIC = struct_acpi_madt_plic;
const struct_unnamed_17 = extern struct {};
const struct_unnamed_16 = extern struct {
    __Empty_OemData: struct_unnamed_17 align(1),
    pub fn OemData(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 0)));
    }
};
pub const struct_acpi_madt_oem_data = extern struct {
    unnamed_0: struct_unnamed_16,
};
pub const ACPI_MADT_OEM_DATA = struct_acpi_madt_oem_data;
pub const struct_acpi_mcfg_allocation = extern struct {
    Address: UINT64,
    PciSegment: UINT16,
    StartBusNumber: UINT8,
    EndBusNumber: UINT8,
    Reserved: UINT32,
};
pub const ACPI_MCFG_ALLOCATION = struct_acpi_mcfg_allocation;
pub const struct_acpi_table_mcfg = extern struct {
    Header: ACPI_TABLE_HEADER align(1),
    Reserved: [8]UINT8,
    pub fn Allocations(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), ACPI_MCFG_ALLOCATION) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), ACPI_MCFG_ALLOCATION);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 44)));
    }
};
pub const ACPI_TABLE_MCFG = struct_acpi_table_mcfg;
pub const struct_acpi_table_mchi = extern struct {
    Header: ACPI_TABLE_HEADER,
    InterfaceType: UINT8,
    Protocol: UINT8,
    ProtocolData: UINT64,
    InterruptType: UINT8,
    Gpe: UINT8,
    PciDeviceFlag: UINT8,
    GlobalInterrupt: UINT32,
    ControlRegister: ACPI_GENERIC_ADDRESS,
    PciSegment: UINT8,
    PciBus: UINT8,
    PciDevice: UINT8,
    PciFunction: UINT8,
};
pub const ACPI_TABLE_MCHI = struct_acpi_table_mchi;
pub const ACPI_MPAM_LOCATION_TYPE_PROCESSOR_CACHE: c_int = 0;
pub const ACPI_MPAM_LOCATION_TYPE_MEMORY: c_int = 1;
pub const ACPI_MPAM_LOCATION_TYPE_SMMU: c_int = 2;
pub const ACPI_MPAM_LOCATION_TYPE_MEMORY_CACHE: c_int = 3;
pub const ACPI_MPAM_LOCATION_TYPE_ACPI_DEVICE: c_int = 4;
pub const ACPI_MPAM_LOCATION_TYPE_INTERCONNECT: c_int = 5;
pub const ACPI_MPAM_LOCATION_TYPE_UNKNOWN: c_int = 255;
pub const enum_AcpiMpamLocatorType = c_uint;
pub const struct_acpi_mpam_func_deps = extern struct {
    Producer: UINT32,
    Reserved: UINT32,
};
pub const ACPI_MPAM_FUNC_DEPS = struct_acpi_mpam_func_deps;
pub const struct_acpi_mpam_resource_cache_locator = extern struct {
    CacheReference: UINT64,
    Reserved: UINT32,
};
pub const ACPI_MPAM_RESOURCE_CACHE_LOCATOR = struct_acpi_mpam_resource_cache_locator;
pub const struct_acpi_mpam_resource_memory_locator = extern struct {
    ProximityDomain: UINT64,
    Reserved: UINT32,
};
pub const ACPI_MPAM_RESOURCE_MEMORY_LOCATOR = struct_acpi_mpam_resource_memory_locator;
pub const struct_acpi_mpam_resource_smmu_locator = extern struct {
    SmmuInterface: UINT64,
    Reserved: UINT32,
};
pub const ACPI_MPAM_RESOURCE_SMMU_INTERFACE = struct_acpi_mpam_resource_smmu_locator;
pub const struct_acpi_mpam_resource_memcache_locator = extern struct {
    Reserved: [7]UINT8,
    Level: UINT8,
    Reference: UINT32,
};
pub const ACPI_MPAM_RESOURCE_MEMCACHE_INTERFACE = struct_acpi_mpam_resource_memcache_locator;
pub const struct_acpi_mpam_resource_acpi_locator = extern struct {
    AcpiHwId: UINT64,
    AcpiUniqueId: UINT32,
};
pub const ACPI_MPAM_RESOURCE_ACPI_INTERFACE = struct_acpi_mpam_resource_acpi_locator;
pub const struct_acpi_mpam_resource_interconnect_locator = extern struct {
    InterConnectDescTblOff: UINT64,
    Reserved: UINT32,
};
pub const ACPI_MPAM_RESOURCE_INTERCONNECT_INTERFACE = struct_acpi_mpam_resource_interconnect_locator;
pub const struct_acpi_mpam_resource_generic_locator = extern struct {
    Descriptor1: UINT64,
    Descriptor2: UINT32,
};
pub const ACPI_MPAM_RESOURCE_GENERIC_LOCATOR = struct_acpi_mpam_resource_generic_locator;
pub const union_acpi_mpam_resource_locator = extern union {
    CacheLocator: ACPI_MPAM_RESOURCE_CACHE_LOCATOR,
    MemoryLocator: ACPI_MPAM_RESOURCE_MEMORY_LOCATOR,
    SmmuLocator: ACPI_MPAM_RESOURCE_SMMU_INTERFACE,
    MemCacheLocator: ACPI_MPAM_RESOURCE_MEMCACHE_INTERFACE,
    AcpiLocator: ACPI_MPAM_RESOURCE_ACPI_INTERFACE,
    InterconnectIfcLocator: ACPI_MPAM_RESOURCE_INTERCONNECT_INTERFACE,
    GenericLocator: ACPI_MPAM_RESOURCE_GENERIC_LOCATOR,
};
pub const ACPI_MPAM_RESOURCE_LOCATOR = union_acpi_mpam_resource_locator;
pub const struct_acpi_mpam_resource_node = extern struct {
    Identifier: UINT32,
    RISIndex: UINT8,
    Reserved1: UINT16,
    LocatorType: UINT8,
    Locator: ACPI_MPAM_RESOURCE_LOCATOR,
    NumFunctionalDeps: UINT32,
};
pub const ACPI_MPAM_RESOURCE_NODE = struct_acpi_mpam_resource_node;
pub const struct_acpi_mpam_msc_node = extern struct {
    Length: UINT16,
    InterfaceType: UINT8,
    Reserved: UINT8,
    Identifier: UINT32,
    BaseAddress: UINT64,
    MMIOSize: UINT32,
    OverflowInterrupt: UINT32,
    OverflowInterruptFlags: UINT32,
    Reserved1: UINT32,
    OverflowInterruptAffinity: UINT32,
    ErrorInterrupt: UINT32,
    ErrorInterruptFlags: UINT32,
    Reserved2: UINT32,
    ErrorInterruptAffinity: UINT32,
    MaxNrdyUsec: UINT32,
    HardwareIdLinkedDevice: UINT64,
    InstanceIdLinkedDevice: UINT32,
    NumResouceNodes: UINT32,
};
pub const ACPI_MPAM_MSC_NODE = struct_acpi_mpam_msc_node;
pub const struct_acpi_table_mpam = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_MPAM = struct_acpi_table_mpam;
pub const struct_acpi_table_mpst = extern struct {
    Header: ACPI_TABLE_HEADER,
    ChannelId: UINT8,
    Reserved1: [3]UINT8,
    PowerNodeCount: UINT16,
    Reserved2: UINT16,
};
pub const ACPI_TABLE_MPST = struct_acpi_table_mpst;
pub const struct_acpi_mpst_channel = extern struct {
    ChannelId: UINT8,
    Reserved1: [3]UINT8,
    PowerNodeCount: UINT16,
    Reserved2: UINT16,
};
pub const ACPI_MPST_CHANNEL = struct_acpi_mpst_channel;
pub const struct_acpi_mpst_power_node = extern struct {
    Flags: UINT8,
    Reserved1: UINT8,
    NodeId: UINT16,
    Length: UINT32,
    RangeAddress: UINT64,
    RangeLength: UINT64,
    NumPowerStates: UINT32,
    NumPhysicalComponents: UINT32,
};
pub const ACPI_MPST_POWER_NODE = struct_acpi_mpst_power_node;
pub const struct_acpi_mpst_power_state = extern struct {
    PowerState: UINT8,
    InfoIndex: UINT8,
};
pub const ACPI_MPST_POWER_STATE = struct_acpi_mpst_power_state;
pub const struct_acpi_mpst_component = extern struct {
    ComponentId: UINT16,
};
pub const ACPI_MPST_COMPONENT = struct_acpi_mpst_component;
pub const struct_acpi_mpst_data_hdr = extern struct {
    CharacteristicsCount: UINT16,
    Reserved: UINT16,
};
pub const ACPI_MPST_DATA_HDR = struct_acpi_mpst_data_hdr;
pub const struct_acpi_mpst_power_data = extern struct {
    StructureId: UINT8,
    Flags: UINT8,
    Reserved1: UINT16,
    AveragePower: UINT32,
    PowerSaving: UINT32,
    ExitLatency: UINT64,
    Reserved2: UINT64,
};
pub const ACPI_MPST_POWER_DATA = struct_acpi_mpst_power_data;
pub const struct_acpi_mpst_shared = extern struct {
    Signature: UINT32,
    PccCommand: UINT16,
    PccStatus: UINT16,
    CommandRegister: UINT32,
    StatusRegister: UINT32,
    PowerStateId: UINT32,
    PowerNodeId: UINT32,
    EnergyConsumed: UINT64,
    AveragePower: UINT64,
};
pub const ACPI_MPST_SHARED = struct_acpi_mpst_shared;
pub const struct_acpi_table_msct = extern struct {
    Header: ACPI_TABLE_HEADER,
    ProximityOffset: UINT32,
    MaxProximityDomains: UINT32,
    MaxClockDomains: UINT32,
    MaxAddress: UINT64,
};
pub const ACPI_TABLE_MSCT = struct_acpi_table_msct;
pub const struct_acpi_msct_proximity = extern struct {
    Revision: UINT8,
    Length: UINT8,
    RangeStart: UINT32,
    RangeEnd: UINT32,
    ProcessorCapacity: UINT32,
    MemoryCapacity: UINT64,
};
pub const ACPI_MSCT_PROXIMITY = struct_acpi_msct_proximity;
pub const struct_acpi_table_msdm = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_MSDM = struct_acpi_table_msdm;
pub const struct_acpi_table_nfit = extern struct {
    Header: ACPI_TABLE_HEADER,
    Reserved: UINT32,
};
pub const ACPI_TABLE_NFIT = struct_acpi_table_nfit;
pub const struct_acpi_nfit_header = extern struct {
    Type: UINT16,
    Length: UINT16,
};
pub const ACPI_NFIT_HEADER = struct_acpi_nfit_header;
pub const ACPI_NFIT_TYPE_SYSTEM_ADDRESS: c_int = 0;
pub const ACPI_NFIT_TYPE_MEMORY_MAP: c_int = 1;
pub const ACPI_NFIT_TYPE_INTERLEAVE: c_int = 2;
pub const ACPI_NFIT_TYPE_SMBIOS: c_int = 3;
pub const ACPI_NFIT_TYPE_CONTROL_REGION: c_int = 4;
pub const ACPI_NFIT_TYPE_DATA_REGION: c_int = 5;
pub const ACPI_NFIT_TYPE_FLUSH_ADDRESS: c_int = 6;
pub const ACPI_NFIT_TYPE_CAPABILITIES: c_int = 7;
pub const ACPI_NFIT_TYPE_RESERVED: c_int = 8;
pub const enum_AcpiNfitType = c_uint;
pub const struct_acpi_nfit_system_address = extern struct {
    Header: ACPI_NFIT_HEADER,
    RangeIndex: UINT16,
    Flags: UINT16,
    Reserved: UINT32,
    ProximityDomain: UINT32,
    RangeGuid: [16]UINT8,
    Address: UINT64,
    Length: UINT64,
    MemoryMapping: UINT64,
    LocationCookie: UINT64,
};
pub const ACPI_NFIT_SYSTEM_ADDRESS = struct_acpi_nfit_system_address;
pub const struct_acpi_nfit_memory_map = extern struct {
    Header: ACPI_NFIT_HEADER,
    DeviceHandle: UINT32,
    PhysicalId: UINT16,
    RegionId: UINT16,
    RangeIndex: UINT16,
    RegionIndex: UINT16,
    RegionSize: UINT64,
    RegionOffset: UINT64,
    Address: UINT64,
    InterleaveIndex: UINT16,
    InterleaveWays: UINT16,
    Flags: UINT16,
    Reserved: UINT16,
};
pub const ACPI_NFIT_MEMORY_MAP = struct_acpi_nfit_memory_map;
pub const struct_acpi_nfit_interleave = extern struct {
    Header: ACPI_NFIT_HEADER align(1),
    InterleaveIndex: UINT16,
    Reserved: UINT16,
    LineCount: UINT32,
    LineSize: UINT32,
    pub fn LineOffset(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT32) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT32);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 16)));
    }
};
pub const ACPI_NFIT_INTERLEAVE = struct_acpi_nfit_interleave;
pub const struct_acpi_nfit_smbios = extern struct {
    Header: ACPI_NFIT_HEADER align(1),
    Reserved: UINT32,
    pub fn Data(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 8)));
    }
};
pub const ACPI_NFIT_SMBIOS = struct_acpi_nfit_smbios;
pub const struct_acpi_nfit_control_region = extern struct {
    Header: ACPI_NFIT_HEADER,
    RegionIndex: UINT16,
    VendorId: UINT16,
    DeviceId: UINT16,
    RevisionId: UINT16,
    SubsystemVendorId: UINT16,
    SubsystemDeviceId: UINT16,
    SubsystemRevisionId: UINT16,
    ValidFields: UINT8,
    ManufacturingLocation: UINT8,
    ManufacturingDate: UINT16,
    Reserved: [2]UINT8,
    SerialNumber: UINT32,
    Code: UINT16,
    Windows: UINT16,
    WindowSize: UINT64,
    CommandOffset: UINT64,
    CommandSize: UINT64,
    StatusOffset: UINT64,
    StatusSize: UINT64,
    Flags: UINT16,
    Reserved1: [6]UINT8,
};
pub const ACPI_NFIT_CONTROL_REGION = struct_acpi_nfit_control_region;
pub const struct_acpi_nfit_data_region = extern struct {
    Header: ACPI_NFIT_HEADER,
    RegionIndex: UINT16,
    Windows: UINT16,
    Offset: UINT64,
    Size: UINT64,
    Capacity: UINT64,
    StartAddress: UINT64,
};
pub const ACPI_NFIT_DATA_REGION = struct_acpi_nfit_data_region;
pub const struct_acpi_nfit_flush_address = extern struct {
    Header: ACPI_NFIT_HEADER align(1),
    DeviceHandle: UINT32,
    HintCount: UINT16,
    Reserved: [6]UINT8,
    pub fn HintAddress(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT64) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT64);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 16)));
    }
};
pub const ACPI_NFIT_FLUSH_ADDRESS = struct_acpi_nfit_flush_address;
pub const struct_acpi_nfit_capabilities = extern struct {
    Header: ACPI_NFIT_HEADER,
    HighestCapability: UINT8,
    Reserved: [3]UINT8,
    Capabilities: UINT32,
    Reserved2: UINT32,
};
pub const ACPI_NFIT_CAPABILITIES = struct_acpi_nfit_capabilities;
pub const struct_nfit_device_handle = extern struct {
    Handle: UINT32,
};
pub const NFIT_DEVICE_HANDLE = struct_nfit_device_handle;
pub const struct_acpi_table_pcct = extern struct {
    Header: ACPI_TABLE_HEADER,
    Flags: UINT32,
    Reserved: UINT64,
};
pub const ACPI_TABLE_PCCT = struct_acpi_table_pcct;
pub const ACPI_PCCT_TYPE_GENERIC_SUBSPACE: c_int = 0;
pub const ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE: c_int = 1;
pub const ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2: c_int = 2;
pub const ACPI_PCCT_TYPE_EXT_PCC_MASTER_SUBSPACE: c_int = 3;
pub const ACPI_PCCT_TYPE_EXT_PCC_SLAVE_SUBSPACE: c_int = 4;
pub const ACPI_PCCT_TYPE_HW_REG_COMM_SUBSPACE: c_int = 5;
pub const ACPI_PCCT_TYPE_RESERVED: c_int = 6;
pub const enum_AcpiPcctType = c_uint;
pub const struct_acpi_pcct_subspace = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Reserved: [6]UINT8,
    BaseAddress: UINT64,
    Length: UINT64,
    DoorbellRegister: ACPI_GENERIC_ADDRESS,
    PreserveMask: UINT64,
    WriteMask: UINT64,
    Latency: UINT32,
    MaxAccessRate: UINT32,
    MinTurnaroundTime: UINT16,
};
pub const ACPI_PCCT_SUBSPACE = struct_acpi_pcct_subspace;
pub const struct_acpi_pcct_hw_reduced = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    PlatformInterrupt: UINT32,
    Flags: UINT8,
    Reserved: UINT8,
    BaseAddress: UINT64,
    Length: UINT64,
    DoorbellRegister: ACPI_GENERIC_ADDRESS,
    PreserveMask: UINT64,
    WriteMask: UINT64,
    Latency: UINT32,
    MaxAccessRate: UINT32,
    MinTurnaroundTime: UINT16,
};
pub const ACPI_PCCT_HW_REDUCED = struct_acpi_pcct_hw_reduced;
pub const struct_acpi_pcct_hw_reduced_type2 = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    PlatformInterrupt: UINT32,
    Flags: UINT8,
    Reserved: UINT8,
    BaseAddress: UINT64,
    Length: UINT64,
    DoorbellRegister: ACPI_GENERIC_ADDRESS,
    PreserveMask: UINT64,
    WriteMask: UINT64,
    Latency: UINT32,
    MaxAccessRate: UINT32,
    MinTurnaroundTime: UINT16,
    PlatformAckRegister: ACPI_GENERIC_ADDRESS,
    AckPreserveMask: UINT64,
    AckWriteMask: UINT64,
};
pub const ACPI_PCCT_HW_REDUCED_TYPE2 = struct_acpi_pcct_hw_reduced_type2;
pub const struct_acpi_pcct_ext_pcc_master = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    PlatformInterrupt: UINT32,
    Flags: UINT8,
    Reserved1: UINT8,
    BaseAddress: UINT64,
    Length: UINT32,
    DoorbellRegister: ACPI_GENERIC_ADDRESS,
    PreserveMask: UINT64,
    WriteMask: UINT64,
    Latency: UINT32,
    MaxAccessRate: UINT32,
    MinTurnaroundTime: UINT32,
    PlatformAckRegister: ACPI_GENERIC_ADDRESS,
    AckPreserveMask: UINT64,
    AckSetMask: UINT64,
    Reserved2: UINT64,
    CmdCompleteRegister: ACPI_GENERIC_ADDRESS,
    CmdCompleteMask: UINT64,
    CmdUpdateRegister: ACPI_GENERIC_ADDRESS,
    CmdUpdatePreserveMask: UINT64,
    CmdUpdateSetMask: UINT64,
    ErrorStatusRegister: ACPI_GENERIC_ADDRESS,
    ErrorStatusMask: UINT64,
};
pub const ACPI_PCCT_EXT_PCC_MASTER = struct_acpi_pcct_ext_pcc_master;
pub const struct_acpi_pcct_ext_pcc_slave = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    PlatformInterrupt: UINT32,
    Flags: UINT8,
    Reserved1: UINT8,
    BaseAddress: UINT64,
    Length: UINT32,
    DoorbellRegister: ACPI_GENERIC_ADDRESS,
    PreserveMask: UINT64,
    WriteMask: UINT64,
    Latency: UINT32,
    MaxAccessRate: UINT32,
    MinTurnaroundTime: UINT32,
    PlatformAckRegister: ACPI_GENERIC_ADDRESS,
    AckPreserveMask: UINT64,
    AckSetMask: UINT64,
    Reserved2: UINT64,
    CmdCompleteRegister: ACPI_GENERIC_ADDRESS,
    CmdCompleteMask: UINT64,
    CmdUpdateRegister: ACPI_GENERIC_ADDRESS,
    CmdUpdatePreserveMask: UINT64,
    CmdUpdateSetMask: UINT64,
    ErrorStatusRegister: ACPI_GENERIC_ADDRESS,
    ErrorStatusMask: UINT64,
};
pub const ACPI_PCCT_EXT_PCC_SLAVE = struct_acpi_pcct_ext_pcc_slave;
pub const struct_acpi_pcct_hw_reg = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Version: UINT16,
    BaseAddress: UINT64,
    Length: UINT64,
    DoorbellRegister: ACPI_GENERIC_ADDRESS,
    DoorbellPreserve: UINT64,
    DoorbellWrite: UINT64,
    CmdCompleteRegister: ACPI_GENERIC_ADDRESS,
    CmdCompleteMask: UINT64,
    ErrorStatusRegister: ACPI_GENERIC_ADDRESS,
    ErrorStatusMask: UINT64,
    NominalLatency: UINT32,
    MinTurnaroundTime: UINT32,
};
pub const ACPI_PCCT_HW_REG = struct_acpi_pcct_hw_reg;
pub const struct_acpi_pcct_shared_memory = extern struct {
    Signature: UINT32,
    Command: UINT16,
    Status: UINT16,
};
pub const ACPI_PCCT_SHARED_MEMORY = struct_acpi_pcct_shared_memory;
pub const struct_acpi_pcct_ext_pcc_shared_memory = extern struct {
    Signature: UINT32,
    Flags: UINT32,
    Length: UINT32,
    Command: UINT32,
};
pub const ACPI_PCCT_EXT_PCC_SHARED_MEMORY = struct_acpi_pcct_ext_pcc_shared_memory;
pub const struct_acpi_table_pdtt = extern struct {
    Header: ACPI_TABLE_HEADER,
    TriggerCount: UINT8,
    Reserved: [3]UINT8,
    ArrayOffset: UINT32,
};
pub const ACPI_TABLE_PDTT = struct_acpi_table_pdtt;
pub const struct_acpi_pdtt_channel = extern struct {
    SubchannelId: UINT8,
    Flags: UINT8,
};
pub const ACPI_PDTT_CHANNEL = struct_acpi_pdtt_channel;
pub const struct_acpi_table_phat = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_PHAT = struct_acpi_table_phat;
pub const struct_acpi_phat_header = extern struct {
    Type: UINT16,
    Length: UINT16,
    Revision: UINT8,
};
pub const ACPI_PHAT_HEADER = struct_acpi_phat_header;
pub const struct_acpi_phat_version_data = extern struct {
    Header: ACPI_PHAT_HEADER,
    Reserved: [3]UINT8,
    ElementCount: UINT32,
};
pub const ACPI_PHAT_VERSION_DATA = struct_acpi_phat_version_data;
pub const struct_acpi_phat_version_element = extern struct {
    Guid: [16]UINT8,
    VersionValue: UINT64,
    ProducerId: UINT32,
};
pub const ACPI_PHAT_VERSION_ELEMENT = struct_acpi_phat_version_element;
pub const struct_acpi_phat_health_data = extern struct {
    Header: ACPI_PHAT_HEADER,
    Reserved: [2]UINT8,
    Health: UINT8,
    DeviceGuid: [16]UINT8,
    DeviceSpecificOffset: UINT32,
};
pub const ACPI_PHAT_HEALTH_DATA = struct_acpi_phat_health_data;
pub const struct_acpi_table_pmtt = extern struct {
    Header: ACPI_TABLE_HEADER,
    MemoryDeviceCount: UINT32,
};
pub const ACPI_TABLE_PMTT = struct_acpi_table_pmtt;
pub const struct_acpi_pmtt_header = extern struct {
    Type: UINT8,
    Reserved1: UINT8,
    Length: UINT16,
    Flags: UINT16,
    Reserved2: UINT16,
    MemoryDeviceCount: UINT32,
};
pub const ACPI_PMTT_HEADER = struct_acpi_pmtt_header;
pub const struct_acpi_pmtt_socket = extern struct {
    Header: ACPI_PMTT_HEADER,
    SocketId: UINT16,
    Reserved: UINT16,
};
pub const ACPI_PMTT_SOCKET = struct_acpi_pmtt_socket;
pub const struct_acpi_pmtt_controller = extern struct {
    Header: ACPI_PMTT_HEADER,
    ControllerId: UINT16,
    Reserved: UINT16,
};
pub const ACPI_PMTT_CONTROLLER = struct_acpi_pmtt_controller;
pub const struct_acpi_pmtt_physical_component = extern struct {
    Header: ACPI_PMTT_HEADER,
    BiosHandle: UINT32,
};
pub const ACPI_PMTT_PHYSICAL_COMPONENT = struct_acpi_pmtt_physical_component;
pub const struct_acpi_pmtt_vendor_specific = extern struct {
    Header: ACPI_PMTT_HEADER align(1),
    TypeUuid: [16]UINT8,
    pub fn Specific(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 28)));
    }
};
pub const ACPI_PMTT_VENDOR_SPECIFIC = struct_acpi_pmtt_vendor_specific;
pub const struct_acpi_table_pptt = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_PPTT = struct_acpi_table_pptt;
pub const ACPI_PPTT_TYPE_PROCESSOR: c_int = 0;
pub const ACPI_PPTT_TYPE_CACHE: c_int = 1;
pub const ACPI_PPTT_TYPE_ID: c_int = 2;
pub const ACPI_PPTT_TYPE_RESERVED: c_int = 3;
pub const enum_AcpiPpttType = c_uint;
pub const struct_acpi_pptt_processor = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Reserved: UINT16,
    Flags: UINT32,
    Parent: UINT32,
    AcpiProcessorId: UINT32,
    NumberOfPrivResources: UINT32,
};
pub const ACPI_PPTT_PROCESSOR = struct_acpi_pptt_processor;
pub const struct_acpi_pptt_cache = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Reserved: UINT16,
    Flags: UINT32,
    NextLevelOfCache: UINT32,
    Size: UINT32,
    NumberOfSets: UINT32,
    Associativity: UINT8,
    Attributes: UINT8,
    LineSize: UINT16,
};
pub const ACPI_PPTT_CACHE = struct_acpi_pptt_cache;
pub const struct_acpi_pptt_cache_v1 = extern struct {
    CacheId: UINT32,
};
pub const ACPI_PPTT_CACHE_V1 = struct_acpi_pptt_cache_v1;
pub const struct_acpi_pptt_id = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Reserved: UINT16,
    VendorId: UINT32,
    Level1Id: UINT64,
    Level2Id: UINT64,
    MajorRev: UINT16,
    MinorRev: UINT16,
    SpinRev: UINT16,
};
pub const ACPI_PPTT_ID = struct_acpi_pptt_id;
pub const struct_acpi_table_prmt = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_PRMT = struct_acpi_table_prmt;
pub const struct_acpi_table_prmt_header = extern struct {
    PlatformGuid: [16]UINT8,
    ModuleInfoOffset: UINT32,
    ModuleInfoCount: UINT32,
};
pub const ACPI_TABLE_PRMT_HEADER = struct_acpi_table_prmt_header;
pub const struct_acpi_prmt_module_header = extern struct {
    Revision: UINT16,
    Length: UINT16,
};
pub const ACPI_PRMT_MODULE_HEADER = struct_acpi_prmt_module_header;
pub const struct_acpi_prmt_module_info = extern struct {
    Revision: UINT16,
    Length: UINT16,
    ModuleGuid: [16]UINT8,
    MajorRev: UINT16,
    MinorRev: UINT16,
    HandlerInfoCount: UINT16,
    HandlerInfoOffset: UINT32,
    MmioListPointer: UINT64,
};
pub const ACPI_PRMT_MODULE_INFO = struct_acpi_prmt_module_info;
pub const struct_acpi_prmt_handler_info = extern struct {
    Revision: UINT16,
    Length: UINT16,
    HandlerGuid: [16]UINT8,
    HandlerAddress: UINT64,
    StaticDataBufferAddress: UINT64,
    AcpiParamBufferAddress: UINT64,
};
pub const ACPI_PRMT_HANDLER_INFO = struct_acpi_prmt_handler_info;
pub const struct_acpi_table_rasf = extern struct {
    Header: ACPI_TABLE_HEADER,
    ChannelId: [12]UINT8,
};
pub const ACPI_TABLE_RASF = struct_acpi_table_rasf;
pub const struct_acpi_rasf_shared_memory = extern struct {
    Signature: UINT32,
    Command: UINT16,
    Status: UINT16,
    Version: UINT16,
    Capabilities: [16]UINT8,
    SetCapabilities: [16]UINT8,
    NumParameterBlocks: UINT16,
    SetCapabilitiesStatus: UINT32,
};
pub const ACPI_RASF_SHARED_MEMORY = struct_acpi_rasf_shared_memory;
pub const struct_acpi_rasf_parameter_block = extern struct {
    Type: UINT16,
    Version: UINT16,
    Length: UINT16,
};
pub const ACPI_RASF_PARAMETER_BLOCK = struct_acpi_rasf_parameter_block;
pub const struct_acpi_rasf_patrol_scrub_parameter = extern struct {
    Header: ACPI_RASF_PARAMETER_BLOCK,
    PatrolScrubCommand: UINT16,
    RequestedAddressRange: [2]UINT64,
    ActualAddressRange: [2]UINT64,
    Flags: UINT16,
    RequestedSpeed: UINT8,
};
pub const ACPI_RASF_PATROL_SCRUB_PARAMETER = struct_acpi_rasf_patrol_scrub_parameter;
pub const ACPI_RASF_EXECUTE_RASF_COMMAND: c_int = 1;
pub const enum_AcpiRasfCommands = c_uint;
pub const ACPI_HW_PATROL_SCRUB_SUPPORTED: c_int = 0;
pub const ACPI_SW_PATROL_SCRUB_EXPOSED: c_int = 1;
pub const enum_AcpiRasfCapabiliities = c_uint;
pub const ACPI_RASF_GET_PATROL_PARAMETERS: c_int = 1;
pub const ACPI_RASF_START_PATROL_SCRUBBER: c_int = 2;
pub const ACPI_RASF_STOP_PATROL_SCRUBBER: c_int = 3;
pub const enum_AcpiRasfPatrolScrubCommands = c_uint;
pub const ACPI_RASF_SUCCESS: c_int = 0;
pub const ACPI_RASF_NOT_VALID: c_int = 1;
pub const ACPI_RASF_NOT_SUPPORTED: c_int = 2;
pub const ACPI_RASF_BUSY: c_int = 3;
pub const ACPI_RASF_FAILED: c_int = 4;
pub const ACPI_RASF_ABORTED: c_int = 5;
pub const ACPI_RASF_INVALID_DATA: c_int = 6;
pub const enum_AcpiRasfStatus = c_uint;
pub const struct_acpi_table_ras2 = extern struct {
    Header: ACPI_TABLE_HEADER,
    Reserved: UINT16,
    NumPccDescs: UINT16,
};
pub const ACPI_TABLE_RAS2 = struct_acpi_table_ras2;
pub const struct_acpi_ras2_pcc_desc = extern struct {
    ChannelId: UINT8,
    Reserved: UINT16,
    FeatureType: UINT8,
    Instance: UINT32,
};
pub const ACPI_RAS2_PCC_DESC = struct_acpi_ras2_pcc_desc;
pub const struct_acpi_ras2_shared_memory = extern struct {
    Signature: UINT32,
    Command: UINT16,
    Status: UINT16,
    Version: UINT16,
    Features: [16]UINT8,
    SetCapabilities: [16]UINT8,
    NumParameterBlocks: UINT16,
    SetCapabilitiesStatus: UINT32,
};
pub const ACPI_RAS2_SHARED_MEMORY = struct_acpi_ras2_shared_memory;
pub const struct_acpi_ras2_parameter_block = extern struct {
    Type: UINT16,
    Version: UINT16,
    Length: UINT16,
};
pub const ACPI_RAS2_PARAMETER_BLOCK = struct_acpi_ras2_parameter_block;
pub const struct_acpi_ras2_patrol_scrub_parameter = extern struct {
    Header: ACPI_RAS2_PARAMETER_BLOCK,
    PatrolScrubCommand: UINT16,
    RequestedAddressRange: [2]UINT64,
    ActualAddressRange: [2]UINT64,
    Flags: UINT32,
    ScrubParamsOut: UINT32,
    ScrubParamsIn: UINT32,
};
pub const ACPI_RAS2_PATROL_SCRUB_PARAMETER = struct_acpi_ras2_patrol_scrub_parameter;
pub const struct_acpi_ras2_la2pa_translation_parameter = extern struct {
    Header: ACPI_RAS2_PARAMETER_BLOCK,
    AddrTranslationCommand: UINT16,
    SubInstId: UINT64,
    LogicalAddress: UINT64,
    PhysicalAddress: UINT64,
    Status: UINT32,
};
pub const ACPI_RAS2_LA2PA_TRANSLATION_PARAM = struct_acpi_ras2_la2pa_translation_parameter;
pub const ACPI_RAS2_EXECUTE_RAS2_COMMAND: c_int = 1;
pub const enum_AcpiRas2Commands = c_uint;
pub const ACPI_RAS2_PATROL_SCRUB_SUPPORTED: c_int = 0;
pub const ACPI_RAS2_LA2PA_TRANSLATION: c_int = 1;
pub const enum_AcpiRas2Features = c_uint;
pub const ACPI_RAS2_GET_PATROL_PARAMETERS: c_int = 1;
pub const ACPI_RAS2_START_PATROL_SCRUBBER: c_int = 2;
pub const ACPI_RAS2_STOP_PATROL_SCRUBBER: c_int = 3;
pub const enum_AcpiRas2PatrolScrubCommands = c_uint;
pub const ACPI_RAS2_GET_LA2PA_TRANSLATION: c_int = 1;
pub const enum_AcpiRas2La2PaTranslationCommands = c_uint;
pub const ACPI_RAS2_LA2PA_TRANSLATION_SUCCESS: c_int = 0;
pub const ACPI_RAS2_LA2PA_TRANSLATION_FAIL: c_int = 1;
pub const enum_AcpiRas2La2PaTranslationStatus = c_uint;
pub const ACPI_RAS2_SUCCESS: c_int = 0;
pub const ACPI_RAS2_NOT_VALID: c_int = 1;
pub const ACPI_RAS2_NOT_SUPPORTED: c_int = 2;
pub const ACPI_RAS2_BUSY: c_int = 3;
pub const ACPI_RAS2_FAILED: c_int = 4;
pub const ACPI_RAS2_ABORTED: c_int = 5;
pub const ACPI_RAS2_INVALID_DATA: c_int = 6;
pub const enum_AcpiRas2Status = c_uint;
pub const struct_acpi_table_rgrt = extern struct {
    Header: ACPI_TABLE_HEADER align(1),
    Version: UINT16,
    ImageType: UINT8,
    Reserved: UINT8,
    pub fn Image(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), UINT8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 40)));
    }
};
pub const ACPI_TABLE_RGRT = struct_acpi_table_rgrt;
pub const ACPI_RGRT_TYPE_RESERVED0: c_int = 0;
pub const ACPI_RGRT_IMAGE_TYPE_PNG: c_int = 1;
pub const ACPI_RGRT_TYPE_RESERVED: c_int = 2;
pub const enum_AcpiRgrtImageType = c_uint;
pub const struct_acpi_table_rhct = extern struct {
    Header: ACPI_TABLE_HEADER,
    Flags: UINT32,
    TimeBaseFreq: UINT64,
    NodeCount: UINT32,
    NodeOffset: UINT32,
};
pub const ACPI_TABLE_RHCT = struct_acpi_table_rhct;
pub const struct_acpi_rhct_node_header = extern struct {
    Type: UINT16,
    Length: UINT16,
    Revision: UINT16,
};
pub const ACPI_RHCT_NODE_HEADER = struct_acpi_rhct_node_header;
pub const ACPI_RHCT_NODE_TYPE_ISA_STRING: c_int = 0;
pub const ACPI_RHCT_NODE_TYPE_CMO: c_int = 1;
pub const ACPI_RHCT_NODE_TYPE_MMU: c_int = 2;
pub const ACPI_RHCT_NODE_TYPE_RESERVED: c_int = 3;
pub const ACPI_RHCT_NODE_TYPE_HART_INFO: c_int = 65535;
pub const enum_acpi_rhct_node_type = c_uint;
pub const struct_acpi_rhct_isa_string = extern struct {
    IsaLength: UINT16 align(1),
    pub fn Isa(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 2)));
    }
};
pub const ACPI_RHCT_ISA_STRING = struct_acpi_rhct_isa_string;
pub const struct_acpi_rhct_cmo_node = extern struct {
    Reserved: UINT8,
    CbomSize: UINT8,
    CbopSize: UINT8,
    CbozSize: UINT8,
};
pub const ACPI_RHCT_CMO_NODE = struct_acpi_rhct_cmo_node;
pub const struct_acpi_rhct_mmu_node = extern struct {
    Reserved: UINT8,
    MmuType: UINT8,
};
pub const ACPI_RHCT_MMU_NODE = struct_acpi_rhct_mmu_node;
pub const ACPI_RHCT_MMU_TYPE_SV39: c_int = 0;
pub const ACPI_RHCT_MMU_TYPE_SV48: c_int = 1;
pub const ACPI_RHCT_MMU_TYPE_SV57: c_int = 2;
pub const enum_acpi_rhct_mmu_type = c_uint;
pub const struct_acpi_rhct_hart_info = extern struct {
    NumOffsets: UINT16,
    Uid: UINT32,
};
pub const ACPI_RHCT_HART_INFO = struct_acpi_rhct_hart_info;
pub const struct_acpi_table_sbst = extern struct {
    Header: ACPI_TABLE_HEADER,
    WarningLevel: UINT32,
    LowLevel: UINT32,
    CriticalLevel: UINT32,
};
pub const ACPI_TABLE_SBST = struct_acpi_table_sbst;
pub const struct_acpi_table_sdei = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_SDEI = struct_acpi_table_sdei;
pub const struct_acpi_table_sdev = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_SDEV = struct_acpi_table_sdev;
pub const struct_acpi_sdev_header = extern struct {
    Type: UINT8,
    Flags: UINT8,
    Length: UINT16,
};
pub const ACPI_SDEV_HEADER = struct_acpi_sdev_header;
pub const ACPI_SDEV_TYPE_NAMESPACE_DEVICE: c_int = 0;
pub const ACPI_SDEV_TYPE_PCIE_ENDPOINT_DEVICE: c_int = 1;
pub const ACPI_SDEV_TYPE_RESERVED: c_int = 2;
pub const enum_AcpiSdevType = c_uint;
pub const struct_acpi_sdev_namespace = extern struct {
    Header: ACPI_SDEV_HEADER,
    DeviceIdOffset: UINT16,
    DeviceIdLength: UINT16,
    VendorDataOffset: UINT16,
    VendorDataLength: UINT16,
};
pub const ACPI_SDEV_NAMESPACE = struct_acpi_sdev_namespace;
pub const struct_acpi_sdev_secure_component = extern struct {
    SecureComponentOffset: UINT16,
    SecureComponentLength: UINT16,
};
pub const ACPI_SDEV_SECURE_COMPONENT = struct_acpi_sdev_secure_component;
pub const struct_acpi_sdev_component = extern struct {
    Header: ACPI_SDEV_HEADER,
};
pub const ACPI_SDEV_COMPONENT = struct_acpi_sdev_component;
pub const ACPI_SDEV_TYPE_ID_COMPONENT: c_int = 0;
pub const ACPI_SDEV_TYPE_MEM_COMPONENT: c_int = 1;
pub const enum_AcpiSacType = c_uint;
pub const struct_acpi_sdev_id_component = extern struct {
    Header: ACPI_SDEV_HEADER,
    HardwareIdOffset: UINT16,
    HardwareIdLength: UINT16,
    SubsystemIdOffset: UINT16,
    SubsystemIdLength: UINT16,
    HardwareRevision: UINT16,
    HardwareRevPresent: UINT8,
    ClassCodePresent: UINT8,
    PciBaseClass: UINT8,
    PciSubClass: UINT8,
    PciProgrammingXface: UINT8,
};
pub const ACPI_SDEV_ID_COMPONENT = struct_acpi_sdev_id_component;
pub const struct_acpi_sdev_mem_component = extern struct {
    Header: ACPI_SDEV_HEADER,
    Reserved: UINT32,
    MemoryBaseAddress: UINT64,
    MemoryLength: UINT64,
};
pub const ACPI_SDEV_MEM_COMPONENT = struct_acpi_sdev_mem_component;
pub const struct_acpi_sdev_pcie = extern struct {
    Header: ACPI_SDEV_HEADER,
    Segment: UINT16,
    StartBus: UINT16,
    PathOffset: UINT16,
    PathLength: UINT16,
    VendorDataOffset: UINT16,
    VendorDataLength: UINT16,
};
pub const ACPI_SDEV_PCIE = struct_acpi_sdev_pcie;
pub const struct_acpi_sdev_pcie_path = extern struct {
    Device: UINT8,
    Function: UINT8,
};
pub const ACPI_SDEV_PCIE_PATH = struct_acpi_sdev_pcie_path;
pub const struct_acpi_table_svkl = extern struct {
    Header: ACPI_TABLE_HEADER,
    Count: UINT32,
};
pub const ACPI_TABLE_SVKL = struct_acpi_table_svkl;
pub const struct_acpi_svkl_key = extern struct {
    Type: UINT16,
    Format: UINT16,
    Size: UINT32,
    Address: UINT64,
};
pub const ACPI_SVKL_KEY = struct_acpi_svkl_key;
pub const ACPI_SVKL_TYPE_MAIN_STORAGE: c_int = 0;
pub const ACPI_SVKL_TYPE_RESERVED: c_int = 1;
pub const enum_acpi_svkl_type = c_uint;
pub const ACPI_SVKL_FORMAT_RAW_BINARY: c_int = 0;
pub const ACPI_SVKL_FORMAT_RESERVED: c_int = 1;
pub const enum_acpi_svkl_format = c_uint;
pub const struct_acpi_table_tdel = extern struct {
    Header: ACPI_TABLE_HEADER,
    Reserved: UINT32,
    LogAreaMinimumLength: UINT64,
    LogAreaStartAddress: UINT64,
};
pub const ACPI_TABLE_TDEL = struct_acpi_table_tdel;
pub const struct_acpi_table_slic = extern struct {
    Header: ACPI_TABLE_HEADER,
};
pub const ACPI_TABLE_SLIC = struct_acpi_table_slic;
pub const struct_acpi_table_slit = extern struct {
    Header: ACPI_TABLE_HEADER,
    LocalityCount: UINT64,
    Entry: [1]UINT8,
};
pub const ACPI_TABLE_SLIT = struct_acpi_table_slit;
pub const struct_acpi_table_spcr = extern struct {
    Header: ACPI_TABLE_HEADER,
    InterfaceType: UINT8,
    Reserved: [3]UINT8,
    SerialPort: ACPI_GENERIC_ADDRESS,
    InterruptType: UINT8,
    PcInterrupt: UINT8,
    Interrupt: UINT32,
    BaudRate: UINT8,
    Parity: UINT8,
    StopBits: UINT8,
    FlowControl: UINT8,
    TerminalType: UINT8,
    Reserved1: UINT8,
    PciDeviceId: UINT16,
    PciVendorId: UINT16,
    PciBus: UINT8,
    PciDevice: UINT8,
    PciFunction: UINT8,
    PciFlags: UINT32,
    PciSegment: UINT8,
    Reserved2: UINT32,
};
pub const ACPI_TABLE_SPCR = struct_acpi_table_spcr;
pub const struct_acpi_table_spmi = extern struct {
    Header: ACPI_TABLE_HEADER,
    InterfaceType: UINT8,
    Reserved: UINT8,
    SpecRevision: UINT16,
    InterruptType: UINT8,
    GpeNumber: UINT8,
    Reserved1: UINT8,
    PciDeviceFlag: UINT8,
    Interrupt: UINT32,
    IpmiRegister: ACPI_GENERIC_ADDRESS,
    PciSegment: UINT8,
    PciBus: UINT8,
    PciDevice: UINT8,
    PciFunction: UINT8,
    Reserved2: UINT8,
};
pub const ACPI_TABLE_SPMI = struct_acpi_table_spmi;
pub const ACPI_SPMI_NOT_USED: c_int = 0;
pub const ACPI_SPMI_KEYBOARD: c_int = 1;
pub const ACPI_SPMI_SMI: c_int = 2;
pub const ACPI_SPMI_BLOCK_TRANSFER: c_int = 3;
pub const ACPI_SPMI_SMBUS: c_int = 4;
pub const ACPI_SPMI_RESERVED: c_int = 5;
pub const enum_AcpiSpmiInterfaceTypes = c_uint;
pub const struct_acpi_table_srat = extern struct {
    Header: ACPI_TABLE_HEADER,
    TableRevision: UINT32,
    Reserved: UINT64,
};
pub const ACPI_TABLE_SRAT = struct_acpi_table_srat;
pub const ACPI_SRAT_TYPE_CPU_AFFINITY: c_int = 0;
pub const ACPI_SRAT_TYPE_MEMORY_AFFINITY: c_int = 1;
pub const ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY: c_int = 2;
pub const ACPI_SRAT_TYPE_GICC_AFFINITY: c_int = 3;
pub const ACPI_SRAT_TYPE_GIC_ITS_AFFINITY: c_int = 4;
pub const ACPI_SRAT_TYPE_GENERIC_AFFINITY: c_int = 5;
pub const ACPI_SRAT_TYPE_GENERIC_PORT_AFFINITY: c_int = 6;
pub const ACPI_SRAT_TYPE_RESERVED: c_int = 7;
pub const enum_AcpiSratType = c_uint;
pub const struct_acpi_srat_cpu_affinity = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    ProximityDomainLo: UINT8,
    ApicId: UINT8,
    Flags: UINT32,
    LocalSapicEid: UINT8,
    ProximityDomainHi: [3]UINT8,
    ClockDomain: UINT32,
};
pub const ACPI_SRAT_CPU_AFFINITY = struct_acpi_srat_cpu_affinity;
pub const struct_acpi_srat_mem_affinity = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    ProximityDomain: UINT32,
    Reserved: UINT16,
    BaseAddress: UINT64,
    Length: UINT64,
    Reserved1: UINT32,
    Flags: UINT32,
    Reserved2: UINT64,
};
pub const ACPI_SRAT_MEM_AFFINITY = struct_acpi_srat_mem_affinity;
pub const struct_acpi_srat_x2apic_cpu_affinity = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Reserved: UINT16,
    ProximityDomain: UINT32,
    ApicId: UINT32,
    Flags: UINT32,
    ClockDomain: UINT32,
    Reserved2: UINT32,
};
pub const ACPI_SRAT_X2APIC_CPU_AFFINITY = struct_acpi_srat_x2apic_cpu_affinity;
pub const struct_acpi_srat_gicc_affinity = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    ProximityDomain: UINT32,
    AcpiProcessorUid: UINT32,
    Flags: UINT32,
    ClockDomain: UINT32,
};
pub const ACPI_SRAT_GICC_AFFINITY = struct_acpi_srat_gicc_affinity;
pub const struct_acpi_srat_gic_its_affinity = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    ProximityDomain: UINT32,
    Reserved: UINT16,
    ItsId: UINT32,
};
pub const ACPI_SRAT_GIC_ITS_AFFINITY = struct_acpi_srat_gic_its_affinity;
pub const struct_acpi_srat_generic_affinity = extern struct {
    Header: ACPI_SUBTABLE_HEADER,
    Reserved: UINT8,
    DeviceHandleType: UINT8,
    ProximityDomain: UINT32,
    DeviceHandle: [16]UINT8,
    Flags: UINT32,
    Reserved1: UINT32,
};
pub const ACPI_SRAT_GENERIC_AFFINITY = struct_acpi_srat_generic_affinity;
pub const struct_acpi_table_stao = extern struct {
    Header: ACPI_TABLE_HEADER,
    IgnoreUart: UINT8,
};
pub const ACPI_TABLE_STAO = struct_acpi_table_stao;
pub const struct_acpi_table_tcpa_hdr = extern struct {
    Header: ACPI_TABLE_HEADER,
    PlatformClass: UINT16,
};
pub const ACPI_TABLE_TCPA_HDR = struct_acpi_table_tcpa_hdr;
pub const struct_acpi_table_tcpa_client = extern struct {
    MinimumLogLength: UINT32,
    LogAddress: UINT64,
};
pub const ACPI_TABLE_TCPA_CLIENT = struct_acpi_table_tcpa_client;
pub const struct_acpi_table_tcpa_server = extern struct {
    Reserved: UINT16,
    MinimumLogLength: UINT64,
    LogAddress: UINT64,
    SpecRevision: UINT16,
    DeviceFlags: UINT8,
    InterruptFlags: UINT8,
    GpeNumber: UINT8,
    Reserved2: [3]UINT8,
    GlobalInterrupt: UINT32,
    Address: ACPI_GENERIC_ADDRESS,
    Reserved3: UINT32,
    ConfigAddress: ACPI_GENERIC_ADDRESS,
    Group: UINT8,
    Bus: UINT8,
    Device: UINT8,
    Function: UINT8,
};
pub const ACPI_TABLE_TCPA_SERVER = struct_acpi_table_tcpa_server;
pub const struct_acpi_table_tpm23 = extern struct {
    Header: ACPI_TABLE_HEADER,
    Reserved: UINT32,
    ControlAddress: UINT64,
    StartMethod: UINT32,
};
pub const ACPI_TABLE_TPM23 = struct_acpi_table_tpm23;
pub const struct_acpi_tmp23_trailer = extern struct {
    Reserved: UINT32,
};
pub const ACPI_TPM23_TRAILER = struct_acpi_tmp23_trailer;
pub const struct_acpi_table_tpm2 = extern struct {
    Header: ACPI_TABLE_HEADER,
    PlatformClass: UINT16,
    Reserved: UINT16,
    ControlAddress: UINT64,
    StartMethod: UINT32,
};
pub const ACPI_TABLE_TPM2 = struct_acpi_table_tpm2;
pub const struct_acpi_tpm2_trailer = extern struct {
    MethodParameters: [12]UINT8,
    MinimumLogLength: UINT32,
    LogAddress: UINT64,
};
pub const ACPI_TPM2_TRAILER = struct_acpi_tpm2_trailer;
pub const struct_acpi_tpm2_arm_smc = extern struct {
    GlobalInterrupt: UINT32,
    InterruptFlags: UINT8,
    OperationFlags: UINT8,
    Reserved: UINT16,
    FunctionId: UINT32,
};
pub const ACPI_TPM2_ARM_SMC = struct_acpi_tpm2_arm_smc;
pub const struct_acpi_table_uefi = extern struct {
    Header: ACPI_TABLE_HEADER,
    Identifier: [16]UINT8,
    DataOffset: UINT16,
};
pub const ACPI_TABLE_UEFI = struct_acpi_table_uefi;
pub const struct_acpi_table_viot = extern struct {
    Header: ACPI_TABLE_HEADER,
    NodeCount: UINT16,
    NodeOffset: UINT16,
    Reserved: [8]UINT8,
};
pub const ACPI_TABLE_VIOT = struct_acpi_table_viot;
pub const struct_acpi_viot_header = extern struct {
    Type: UINT8,
    Reserved: UINT8,
    Length: UINT16,
};
pub const ACPI_VIOT_HEADER = struct_acpi_viot_header;
pub const ACPI_VIOT_NODE_PCI_RANGE: c_int = 1;
pub const ACPI_VIOT_NODE_MMIO: c_int = 2;
pub const ACPI_VIOT_NODE_VIRTIO_IOMMU_PCI: c_int = 3;
pub const ACPI_VIOT_NODE_VIRTIO_IOMMU_MMIO: c_int = 4;
pub const ACPI_VIOT_RESERVED: c_int = 5;
pub const enum_AcpiViotNodeType = c_uint;
pub const struct_acpi_viot_pci_range = extern struct {
    Header: ACPI_VIOT_HEADER,
    EndpointStart: UINT32,
    SegmentStart: UINT16,
    SegmentEnd: UINT16,
    BdfStart: UINT16,
    BdfEnd: UINT16,
    OutputNode: UINT16,
    Reserved: [6]UINT8,
};
pub const ACPI_VIOT_PCI_RANGE = struct_acpi_viot_pci_range;
pub const struct_acpi_viot_mmio = extern struct {
    Header: ACPI_VIOT_HEADER,
    Endpoint: UINT32,
    BaseAddress: UINT64,
    OutputNode: UINT16,
    Reserved: [6]UINT8,
};
pub const ACPI_VIOT_MMIO = struct_acpi_viot_mmio;
pub const struct_acpi_viot_virtio_iommu_pci = extern struct {
    Header: ACPI_VIOT_HEADER,
    Segment: UINT16,
    Bdf: UINT16,
    Reserved: [8]UINT8,
};
pub const ACPI_VIOT_VIRTIO_IOMMU_PCI = struct_acpi_viot_virtio_iommu_pci;
pub const struct_acpi_viot_virtio_iommu_mmio = extern struct {
    Header: ACPI_VIOT_HEADER,
    Reserved: [4]UINT8,
    BaseAddress: UINT64,
};
pub const ACPI_VIOT_VIRTIO_IOMMU_MMIO = struct_acpi_viot_virtio_iommu_mmio;
pub const struct_acpi_table_waet = extern struct {
    Header: ACPI_TABLE_HEADER,
    Flags: UINT32,
};
pub const ACPI_TABLE_WAET = struct_acpi_table_waet;
pub const struct_acpi_table_wdat = extern struct {
    Header: ACPI_TABLE_HEADER,
    HeaderLength: UINT32,
    PciSegment: UINT16,
    PciBus: UINT8,
    PciDevice: UINT8,
    PciFunction: UINT8,
    Reserved: [3]UINT8,
    TimerPeriod: UINT32,
    MaxCount: UINT32,
    MinCount: UINT32,
    Flags: UINT8,
    Reserved2: [3]UINT8,
    Entries: UINT32,
};
pub const ACPI_TABLE_WDAT = struct_acpi_table_wdat;
pub const struct_acpi_wdat_entry = extern struct {
    Action: UINT8,
    Instruction: UINT8,
    Reserved: UINT16,
    RegisterRegion: ACPI_GENERIC_ADDRESS,
    Value: UINT32,
    Mask: UINT32,
};
pub const ACPI_WDAT_ENTRY = struct_acpi_wdat_entry;
pub const ACPI_WDAT_RESET: c_int = 1;
pub const ACPI_WDAT_GET_CURRENT_COUNTDOWN: c_int = 4;
pub const ACPI_WDAT_GET_COUNTDOWN: c_int = 5;
pub const ACPI_WDAT_SET_COUNTDOWN: c_int = 6;
pub const ACPI_WDAT_GET_RUNNING_STATE: c_int = 8;
pub const ACPI_WDAT_SET_RUNNING_STATE: c_int = 9;
pub const ACPI_WDAT_GET_STOPPED_STATE: c_int = 10;
pub const ACPI_WDAT_SET_STOPPED_STATE: c_int = 11;
pub const ACPI_WDAT_GET_REBOOT: c_int = 16;
pub const ACPI_WDAT_SET_REBOOT: c_int = 17;
pub const ACPI_WDAT_GET_SHUTDOWN: c_int = 18;
pub const ACPI_WDAT_SET_SHUTDOWN: c_int = 19;
pub const ACPI_WDAT_GET_STATUS: c_int = 32;
pub const ACPI_WDAT_SET_STATUS: c_int = 33;
pub const ACPI_WDAT_ACTION_RESERVED: c_int = 34;
pub const enum_AcpiWdatActions = c_uint;
pub const ACPI_WDAT_READ_VALUE: c_int = 0;
pub const ACPI_WDAT_READ_COUNTDOWN: c_int = 1;
pub const ACPI_WDAT_WRITE_VALUE: c_int = 2;
pub const ACPI_WDAT_WRITE_COUNTDOWN: c_int = 3;
pub const ACPI_WDAT_INSTRUCTION_RESERVED: c_int = 4;
pub const ACPI_WDAT_PRESERVE_REGISTER: c_int = 128;
pub const enum_AcpiWdatInstructions = c_uint;
pub const struct_acpi_table_wddt = extern struct {
    Header: ACPI_TABLE_HEADER,
    SpecVersion: UINT16,
    TableVersion: UINT16,
    PciVendorId: UINT16,
    Address: ACPI_GENERIC_ADDRESS,
    MaxCount: UINT16,
    MinCount: UINT16,
    Period: UINT16,
    Status: UINT16,
    Capability: UINT16,
};
pub const ACPI_TABLE_WDDT = struct_acpi_table_wddt;
pub const struct_acpi_table_wdrt = extern struct {
    Header: ACPI_TABLE_HEADER,
    ControlRegister: ACPI_GENERIC_ADDRESS,
    CountRegister: ACPI_GENERIC_ADDRESS,
    PciDeviceId: UINT16,
    PciVendorId: UINT16,
    PciBus: UINT8,
    PciDevice: UINT8,
    PciFunction: UINT8,
    PciSegment: UINT8,
    MaxCount: UINT16,
    Units: UINT8,
};
pub const ACPI_TABLE_WDRT = struct_acpi_table_wdrt;
pub const struct_acpi_table_wpbt = extern struct {
    Header: ACPI_TABLE_HEADER,
    HandoffSize: UINT32,
    HandoffAddress: UINT64,
    Layout: UINT8,
    Type: UINT8,
    ArgumentsLength: UINT16,
};
pub const ACPI_TABLE_WPBT = struct_acpi_table_wpbt;
pub const struct_acpi_wpbt_unicode = extern struct {
    UnicodeString: [*c]UINT16,
};
pub const ACPI_WPBT_UNICODE = struct_acpi_wpbt_unicode;
pub const struct_acpi_table_wsmt = extern struct {
    Header: ACPI_TABLE_HEADER,
    ProtectionFlags: UINT32,
};
pub const ACPI_TABLE_WSMT = struct_acpi_table_wsmt;
pub const struct_acpi_table_xenv = extern struct {
    Header: ACPI_TABLE_HEADER,
    GrantTableAddress: UINT64,
    GrantTableSize: UINT64,
    EventInterrupt: UINT32,
    EventFlags: UINT8,
};
pub const ACPI_TABLE_XENV = struct_acpi_table_xenv;
pub const acpi_std_hdr_t = ACPI_TABLE_HEADER;
pub const acpi_gen_addr_t = ACPI_GENERIC_ADDRESS;
pub const acpi_rsdp_t = ACPI_RSDP_COMMON;
pub const acpi_xsdp_t = ACPI_RSDP_EXTENSION;
pub const acpi_tbl_rsdp_t = ACPI_TABLE_RSDP;
pub const acpi_tbl_rsdt_t = ACPI_TABLE_RSDT;
pub const acpi_tbl_desc_t = ACPI_TABLE_DESC;
pub const apci_tbl_facs_t = ACPI_TABLE_FACS;
pub const acpi_tbl_fadt_t = ACPI_TABLE_FADT;
pub const acpi_tbl_madt_t = ACPI_TABLE_MADT;
pub const acpi_tbl_mcfg_t = ACPI_TABLE_MCFG;
pub const acpi_mcfg_entry_t = ACPI_MCFG_ALLOCATION;
pub extern fn acpi_add_device(handle: acpi_handle_t, @"type": c_int) kerror_t;
pub const DEVICE_ITTERATE = ?*const fn ([*c]struct_device) callconv(.C) @"bool";
pub const device_t = struct_device;
pub fn device_has_endpoint(arg_device_1: [*c]device_t, arg_type: enum_ENDPOINT_TYPE) callconv(.C) @"bool" {
    var device_1 = arg_device_1;
    var @"type" = arg_type;
    if (!(device_1.*.endpoints != null)) return 0;
    {
        var i: u32 = 0;
        while (i < device_1.*.endpoint_count) : (i +%= 1) if (device_1.*.endpoints[i].type == @"type") return 1;
    }
    return 0;
}
pub fn device_get_endpoint(arg_device_1: [*c]device_t, arg_type: enum_ENDPOINT_TYPE) callconv(.C) [*c]struct_device_endpoint {
    var device_1 = arg_device_1;
    var @"type" = arg_type;
    if (!(device_1.*.endpoints != null)) return null;
    {
        var i: u32 = 0;
        while (i < device_1.*.endpoint_count) : (i +%= 1) {
            if (device_1.*.endpoints[i].type == @"type") return &device_1.*.endpoints[i];
        }
    }
    return null;
}
pub fn device_is_bus(arg_dev: [*c]device_t) callconv(.C) @"bool" {
    var dev = arg_dev;
    return @as(@"bool", @intFromBool(dev.*.bus_group != @as([*c]struct_dgroup, @ptrCast(@alignCast(@as(?*anyopaque, @ptrFromInt(@as(c_int, 0))))))));
}
pub extern fn init_devices(...) void;
pub extern fn debug_devices(...) void;
pub extern fn create_device(parent: [*c]struct_aniva_driver, name: [*c]u8) [*c]device_t;
pub extern fn create_device_ex(parent: [*c]struct_aniva_driver, name: [*c]u8, priv: ?*anyopaque, flags: u32, eps: [*c]struct_device_endpoint, ep_count: u32) [*c]device_t;
pub extern fn destroy_device(device: [*c]device_t) void;
pub extern fn device_register(dev: [*c]device_t, group: [*c]struct_dgroup) c_int;
pub extern fn device_register_to_bus(dev: [*c]device_t, busdev: [*c]device_t) c_int;
pub extern fn device_unregister(dev: [*c]device_t) c_int;
pub extern fn device_get_group(dev: [*c]device_t, group: [*c][*c]struct_dgroup) c_int;
pub extern fn device_node_add_group(node: ?*struct_oss_node, group: [*c]struct_dgroup) c_int;
pub extern fn device_for_each(root: [*c]struct_dgroup, callback: DEVICE_ITTERATE) c_int;
pub extern fn device_open(path: [*c]const u8) [*c]device_t;
pub extern fn device_close(dev: [*c]device_t) c_int;
pub extern fn device_bind_driver(dev: [*c]device_t, driver: [*c]struct_drv_manifest) kerror_t;
pub extern fn device_read(dev: [*c]device_t, buffer: ?*anyopaque, offset: usize, size: usize) c_int;
pub extern fn device_write(dev: [*c]device_t, buffer: ?*anyopaque, offset: usize, size: usize) c_int;
pub extern fn device_message(dev: [*c]device_t, code: dcc_t) usize;
pub extern fn device_message_ex(dev: [*c]device_t, code: dcc_t, buffer: ?*anyopaque, size: usize, out_buffer: ?*anyopaque, out_size: usize) usize;
pub extern fn device_power_on(dev: [*c]device_t) c_int;
pub extern fn device_on_remove(dev: [*c]device_t) c_int;
pub extern fn device_suspend(dev: [*c]device_t) c_int;
pub extern fn device_resume(dev: [*c]device_t) c_int;
pub const drv_dependency_t = struct_drv_dependency;
pub fn drv_dep_is_driver(arg_dep: [*c]drv_dependency_t) callconv(.C) @"bool" {
    var dep = arg_dep;
    return @as(@"bool", @intFromBool((dep.*.type == @as(c_uint, @bitCast(DRV_DEPTYPE_PATH))) or (dep.*.type == @as(c_uint, @bitCast(DRV_DEPTYPE_URL)))));
}
pub fn drv_dep_is_optional(arg_dep: [*c]drv_dependency_t) callconv(.C) @"bool" {
    var dep = arg_dep;
    return @as(@"bool", @intFromBool((dep.*.flags & @as(u32, @bitCast(@as(c_int, 2)))) == @as(u32, @bitCast(@as(c_int, 2)))));
}
pub extern fn driver_is_ready(manifest: [*c]struct_drv_manifest) @"bool";
pub extern fn driver_is_busy(manifest: [*c]struct_drv_manifest) @"bool";
pub extern fn drv_read(manifest: [*c]struct_drv_manifest, buffer: ?*anyopaque, buffer_size: [*c]usize, offset: usize) c_int;
pub extern fn drv_write(manifest: [*c]struct_drv_manifest, buffer: ?*anyopaque, buffer_size: [*c]usize, offset: usize) c_int;
pub extern fn bootstrap_driver(manifest: [*c]struct_drv_manifest) ErrorOrPtr;
pub extern fn in8(port: u16) u8;
pub extern fn in16(port: u16) u16;
pub extern fn in32(port: u16) u32;
pub extern fn out8(port: u16, value: u8) void;
pub extern fn out16(port: u16, value: u16) void;
pub extern fn out32(port: u16, value: u32) void;
pub extern fn io_delay(...) void;
pub extern fn udelay(microseconds: usize) void;
pub extern fn mmio_read_byte(address: ?*anyopaque) u8;
pub extern fn mmio_read_word(address: ?*anyopaque) u16;
pub extern fn mmio_read_dword(address: ?*anyopaque) u32;
pub extern fn mmio_read_qword(address: ?*anyopaque) u64;
pub extern fn mmio_write_byte(address: ?*anyopaque, value: u8) void;
pub extern fn mmio_write_word(address: ?*anyopaque, value: u16) void;
pub extern fn mmio_write_dword(address: ?*anyopaque, value: u32) void;
pub extern fn mmio_write_qword(address: ?*anyopaque, value: usize) void;
pub extern fn init_serial(...) void;
pub const page_dir_t = extern struct {
    m_kernel_high: vaddr_t,
    m_kernel_low: vaddr_t,
    m_root: [*c]pml_entry_t,
};
pub const MALLOC_NODE_FLAG_USED: c_int = 1;
pub const MALLOC_NODE_FLAG_READONLY: c_int = 2;
pub const enum_MALLOC_NODE_FLAGS = c_uint;
pub const MALLOC_NODE_FLAGS_t = enum_MALLOC_NODE_FLAGS;
pub const struct_heap_node = extern struct {
    size: usize align(8),
    next: [*c]struct_heap_node,
    prev: [*c]struct_heap_node,
    flags: u32,
    identifier: u32,
    pub fn data(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 32)));
    }
};
pub const heap_node_t = struct_heap_node;
pub const struct_heap_node_buffer = extern struct {
    m_node_count: usize align(8),
    m_buffer_size: usize,
    m_next: [*c]struct_heap_node_buffer,
    m_last_free_node: [*c]heap_node_t,
    pub fn m_start_node(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), heap_node_t) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), heap_node_t);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 32)));
    }
};
pub const heap_node_buffer_t = struct_heap_node_buffer;
pub const struct_memory_allocator = extern struct {
    m_flags: u32,
    m_buffer_count: u32,
    m_parent_dir: page_dir_t,
    m_buffers: [*c]heap_node_buffer_t,
    m_free_size: usize,
    m_used_size: usize,
};
pub const memory_allocator_t = struct_memory_allocator;
pub extern fn create_malloc_heap(size: usize, virtual_base: vaddr_t, flags: usize) [*c]memory_allocator_t;
pub extern fn create_malloc_heap_ex(dir: [*c]page_dir_t, size: usize, virtual_base: vaddr_t, flags: usize) [*c]memory_allocator_t;
pub extern fn memory_allocate(allocator: [*c]memory_allocator_t, bytes: usize) ?*anyopaque;
pub extern fn memory_sized_deallocate(allocator: [*c]memory_allocator_t, addr: ?*anyopaque, allocation_size: usize) void;
pub extern fn memory_deallocate(allocator: [*c]memory_allocator_t, addr: ?*anyopaque) void;
pub extern fn memory_try_heap_expand(allocator: [*c]memory_allocator_t, new_size: usize) ANIVA_STATUS;
pub extern fn quick_print_node_sizes(allocator: [*c]memory_allocator_t) void;
pub extern fn memory_get_heapnode_at(buffer: [*c]heap_node_buffer_t, index: u32) [*c]heap_node_t;
pub extern fn init_kheap(...) void;
pub extern fn kheap_enable_expand(...) void;
pub extern fn kmalloc(len: usize) ?*anyopaque;
pub extern fn kfree(addr: ?*anyopaque) void;
pub extern fn kfree_sized(addr: ?*anyopaque, allocation_size: usize) void;
pub extern fn kheap_ensure_size(size: usize) void;
pub extern fn kheap_copy_main_allocator(alloc: [*c]memory_allocator_t) c_int;
pub const struct_refc = extern struct {
    m_count: flat_refc_t,
    m_referenced_handle: ?*anyopaque,
    m_destructor: ?*const fn (?*anyopaque) callconv(.C) void,
};
pub const refc_t = struct_refc;
pub extern fn init_refc(ref: [*c]refc_t, destructor: FuncPtr, handle: ?*anyopaque) void;
pub extern fn create_refc(destructor: FuncPtr, referenced_handle: ?*anyopaque) [*c]refc_t;
pub extern fn destroy_refc(ref: [*c]refc_t) void;
pub inline fn is_referenced(arg_ref_1: [*c]refc_t) @"bool" {
    var ref_1 = arg_ref_1;
    return @as(@"bool", @intFromBool(ref_1.*.m_count != @as(c_int, 0)));
}
pub inline fn flat_ref(arg_frc_p: [*c]flat_refc_t) void {
    var frc_p = arg_frc_p;
    frc_p.* += @as(c_int, 1);
}
pub inline fn flat_unref(arg_frc_p: [*c]flat_refc_t) void {
    var frc_p = arg_frc_p;
    if (!(frc_p != null) or !(frc_p.* != 0)) {
        return;
    }
    frc_p.* -= 1;
}
pub inline fn ref(arg_rc: [*c]refc_t) void {
    var rc = arg_rc;
    rc.*.m_count += 1;
}
pub extern fn unref(rc: [*c]refc_t) void;
pub extern fn init_kresources(...) void;
pub fn resource_contains(arg_resource: [*c]kresource_t, arg_address: usize) callconv(.C) @"bool" {
    var resource = arg_resource;
    var address = arg_address;
    if (!(resource != null)) return 0;
    return @as(@"bool", @intFromBool((address >= resource.*.m_start) and (address < (resource.*.m_start +% resource.*.m_size))));
}
pub fn resource_is_anonymous(arg_resource: [*c]kresource_t) callconv(.C) @"bool" {
    var resource = arg_resource;
    return @as(@"bool", @intFromBool(!(resource.*.m_owner != null) and (resource.*.m_shared_count != 0)));
}
pub extern fn create_resource_bundle(dir: [*c]page_dir_t) [*c]kresource_bundle_t;
pub extern fn destroy_resource_bundle(bundle: [*c]kresource_bundle_t) void;
pub extern fn destroy_kresource(resource: [*c]kresource_t) void;
pub extern fn resource_claim_ex(name: [*c]const u8, owner: ?*anyopaque, start: usize, size: usize, @"type": kresource_type_t, bundle: [*c]kresource_bundle_t) ErrorOrPtr;
pub extern fn resource_claim_kernel(name: [*c]const u8, owner: ?*anyopaque, start: usize, size: usize, @"type": kresource_type_t) ErrorOrPtr;
pub extern fn resource_commit(start: usize, size: usize, @"type": kresource_type_t, bundle: [*c]kresource_bundle_t) ErrorOrPtr;
pub extern fn resource_apply_flags(start: usize, size: usize, flags: kresource_flags_t, resources_start: [*c]kresource_t) ErrorOrPtr;
pub extern fn resource_release_region(previous_region: [*c]kresource_t, current_region: [*c]kresource_t) ErrorOrPtr;
pub extern fn resource_release(start: usize, size: usize, mirrors_start: [*c]kresource_t) ErrorOrPtr;
pub extern fn resource_clear_owned(owner: ?*anyopaque, @"type": kresource_type_t, bundle: [*c]kresource_bundle_t) ErrorOrPtr;
pub extern fn resource_find_usable_range(bundle: [*c]kresource_bundle_t, @"type": kresource_type_t, size: usize) ErrorOrPtr;
pub const hashmap_key_t = ?*anyopaque;
pub const hashmap_value_t = ?*anyopaque;
pub const struct_hashmap_entry = opaque {};
pub const struct___hashmap = extern struct {
    m_max_entries: usize align(8),
    m_size: usize,
    f_hash_func: ?*const fn (hashmap_key_t) callconv(.C) usize,
    m_flags: u32,
    pub fn m_list(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), hashmap_value_t) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), hashmap_value_t);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 32)));
    }
};
pub const hashmap_t = struct___hashmap;
pub const hashmap_itterate_fn_t = ?*const fn (hashmap_value_t, u64, u64) callconv(.C) ErrorOrPtr;
pub extern fn init_hashmap(...) void;
pub extern fn create_hashmap(max_entries: usize, flags: u32) [*c]hashmap_t;
pub extern fn destroy_hashmap(map: [*c]hashmap_t) void;
pub extern fn hashmap_itterate(map: [*c]hashmap_t, @"fn": hashmap_itterate_fn_t, arg0: u64, arg1: u64) ErrorOrPtr;
pub extern fn hashmap_put(map: [*c]hashmap_t, key: hashmap_key_t, value: hashmap_value_t) ErrorOrPtr;
pub extern fn hashmap_set(map: [*c]hashmap_t, key: hashmap_key_t, value: hashmap_value_t) ErrorOrPtr;
pub extern fn hashmap_remove(map: [*c]hashmap_t, key: hashmap_key_t) hashmap_value_t;
pub extern fn hashmap_get(map: [*c]hashmap_t, key: hashmap_key_t) hashmap_value_t;
pub extern fn hashmap_to_array(map: [*c]hashmap_t, array_ptr: [*c][*c]usize, size_ptr: [*c]usize) c_int;
pub fn hashmap_has(arg_map: [*c]hashmap_t, arg_key: hashmap_key_t) callconv(.C) @"bool" {
    var map = arg_map;
    var key = arg_key;
    return @as(@"bool", @intFromBool(hashmap_get(map, key) != @as(?*anyopaque, @ptrFromInt(@as(c_int, 0)))));
}
const union_unnamed_18 = extern union {
    drv: [*c]struct_drv_manifest,
    proc: ?*struct_proc,
};
pub const struct_manifest_dependency = extern struct {
    dep: drv_dependency_t,
    obj: union_unnamed_18,
};
pub const manifest_dependency_t = struct_manifest_dependency;
pub const drv_manifest_t = struct_drv_manifest;
pub extern fn create_drv_manifest(handle: [*c]aniva_driver_t) [*c]drv_manifest_t;
pub extern fn destroy_drv_manifest(manifest: [*c]drv_manifest_t) void;
pub extern fn manifest_gather_dependencies(manifest: [*c]drv_manifest_t) c_int;
pub extern fn is_manifest_valid(manifest: [*c]drv_manifest_t) @"bool";
pub extern fn manifest_emplace_handle(manifest: [*c]drv_manifest_t, handle: [*c]aniva_driver_t) ErrorOrPtr;
pub extern fn driver_manifest_write(manifest: [*c]struct_aniva_driver, write_fn: ?*const fn (...) callconv(.C) c_int) @"bool";
pub extern fn driver_manifest_read(manifest: [*c]struct_aniva_driver, read_fn: ?*const fn (...) callconv(.C) c_int) @"bool";
pub extern fn manifest_add_dev(driver: [*c]struct_drv_manifest) void;
pub extern fn manifest_remove_dev(driver: [*c]struct_drv_manifest) void;
pub fn manifest_is_active(arg_manifest: [*c]drv_manifest_t) callconv(.C) @"bool" {
    var manifest = arg_manifest;
    return @as(@"bool", @intFromBool((manifest.*.m_flags & @as(u32, @bitCast(@as(c_int, 4)))) == @as(u32, @bitCast(@as(c_int, 4)))));
}
pub extern fn init_multiboot(addr: ?*anyopaque) ErrorOrPtr;
pub extern fn finalize_multiboot(...) ErrorOrPtr;
pub extern fn get_total_mb2_size(start_addr: ?*anyopaque) usize;
pub extern fn next_mb2_tag(cur: ?*anyopaque, @"type": u32) ?*anyopaque;
pub extern fn get_mb2_tag(addr: ?*anyopaque, @"type": u32) ?*anyopaque;
pub const multiboot_uint8_t = u8;
pub const multiboot_uint16_t = c_ushort;
pub const multiboot_uint32_t = c_uint;
pub const multiboot_uint64_t = c_ulonglong;
pub const struct_multiboot_header = extern struct {
    magic: multiboot_uint32_t,
    architecture: multiboot_uint32_t,
    header_length: multiboot_uint32_t,
    checksum: multiboot_uint32_t,
};
pub const struct_multiboot_header_tag = extern struct {
    type: multiboot_uint16_t,
    flags: multiboot_uint16_t,
    size: multiboot_uint32_t,
};
pub const struct_multiboot_header_tag_information_request = extern struct {
    type: multiboot_uint16_t align(4),
    flags: multiboot_uint16_t,
    size: multiboot_uint32_t,
    pub fn requests(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint32_t) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint32_t);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 8)));
    }
};
pub const struct_multiboot_header_tag_address = extern struct {
    type: multiboot_uint16_t,
    flags: multiboot_uint16_t,
    size: multiboot_uint32_t,
    header_addr: multiboot_uint32_t,
    load_addr: multiboot_uint32_t,
    load_end_addr: multiboot_uint32_t,
    bss_end_addr: multiboot_uint32_t,
};
pub const struct_multiboot_header_tag_entry_address = extern struct {
    type: multiboot_uint16_t,
    flags: multiboot_uint16_t,
    size: multiboot_uint32_t,
    entry_addr: multiboot_uint32_t,
};
pub const struct_multiboot_header_tag_console_flags = extern struct {
    type: multiboot_uint16_t,
    flags: multiboot_uint16_t,
    size: multiboot_uint32_t,
    console_flags: multiboot_uint32_t,
};
pub const struct_multiboot_header_tag_framebuffer = extern struct {
    type: multiboot_uint16_t,
    flags: multiboot_uint16_t,
    size: multiboot_uint32_t,
    width: multiboot_uint32_t,
    height: multiboot_uint32_t,
    depth: multiboot_uint32_t,
};
pub const struct_multiboot_header_tag_module_align = extern struct {
    type: multiboot_uint16_t,
    flags: multiboot_uint16_t,
    size: multiboot_uint32_t,
};
pub const struct_multiboot_header_tag_relocatable = extern struct {
    type: multiboot_uint16_t,
    flags: multiboot_uint16_t,
    size: multiboot_uint32_t,
    min_addr: multiboot_uint32_t,
    max_addr: multiboot_uint32_t,
    @"align": multiboot_uint32_t,
    preference: multiboot_uint32_t,
};
pub const struct_multiboot_color = extern struct {
    red: multiboot_uint8_t,
    green: multiboot_uint8_t,
    blue: multiboot_uint8_t,
};
pub const struct_multiboot_mmap_entry = extern struct {
    addr: multiboot_uint64_t,
    len: multiboot_uint64_t,
    type: multiboot_uint32_t,
    zero: multiboot_uint32_t,
};
pub const multiboot_memory_map_t = struct_multiboot_mmap_entry;
pub const struct_multiboot_tag = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
};
pub const struct_multiboot_tag_string = extern struct {
    type: multiboot_uint32_t align(4),
    size: multiboot_uint32_t,
    pub fn string(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 8)));
    }
};
pub const struct_multiboot_tag_module = extern struct {
    type: multiboot_uint32_t align(4),
    size: multiboot_uint32_t,
    mod_start: multiboot_uint32_t,
    mod_end: multiboot_uint32_t,
    pub fn cmdline(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 16)));
    }
};
pub const struct_multiboot_tag_basic_meminfo = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
    mem_lower: multiboot_uint32_t,
    mem_upper: multiboot_uint32_t,
};
pub const struct_multiboot_tag_bootdev = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
    biosdev: multiboot_uint32_t,
    slice: multiboot_uint32_t,
    part: multiboot_uint32_t,
};
pub const struct_multiboot_tag_mmap = extern struct {
    type: multiboot_uint32_t align(8),
    size: multiboot_uint32_t,
    entry_size: multiboot_uint32_t,
    entry_version: multiboot_uint32_t,
    pub fn entries(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), struct_multiboot_mmap_entry) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), struct_multiboot_mmap_entry);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 16)));
    }
};
pub const struct_multiboot_vbe_info_block = extern struct {
    external_specification: [512]multiboot_uint8_t,
};
pub const struct_multiboot_vbe_mode_info_block = extern struct {
    external_specification: [256]multiboot_uint8_t,
};
pub const struct_multiboot_tag_vbe = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
    vbe_mode: multiboot_uint16_t,
    vbe_interface_seg: multiboot_uint16_t,
    vbe_interface_off: multiboot_uint16_t,
    vbe_interface_len: multiboot_uint16_t,
    vbe_control_info: struct_multiboot_vbe_info_block,
    vbe_mode_info: struct_multiboot_vbe_mode_info_block,
};
pub const struct_multiboot_tag_framebuffer_common = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
    framebuffer_addr: multiboot_uint64_t,
    framebuffer_pitch: multiboot_uint32_t,
    framebuffer_width: multiboot_uint32_t,
    framebuffer_height: multiboot_uint32_t,
    framebuffer_bpp: multiboot_uint8_t,
    framebuffer_type: multiboot_uint8_t,
    reserved: multiboot_uint16_t,
};
const struct_unnamed_20 = extern struct {
    framebuffer_palette_num_colors: multiboot_uint16_t align(2),
    pub fn framebuffer_palette(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), struct_multiboot_color) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), struct_multiboot_color);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 2)));
    }
};
const struct_unnamed_21 = extern struct {
    framebuffer_red_field_position: multiboot_uint8_t,
    framebuffer_red_mask_size: multiboot_uint8_t,
    framebuffer_green_field_position: multiboot_uint8_t,
    framebuffer_green_mask_size: multiboot_uint8_t,
    framebuffer_blue_field_position: multiboot_uint8_t,
    framebuffer_blue_mask_size: multiboot_uint8_t,
};
const union_unnamed_19 = extern union {
    unnamed_0: struct_unnamed_20,
    unnamed_1: struct_unnamed_21,
};
pub const struct_multiboot_tag_framebuffer = extern struct {
    common: struct_multiboot_tag_framebuffer_common,
    unnamed_0: union_unnamed_19,
};
pub const struct_multiboot_tag_elf_sections = extern struct {
    type: multiboot_uint32_t align(4),
    size: multiboot_uint32_t,
    num: multiboot_uint32_t,
    entsize: multiboot_uint32_t,
    shndx: multiboot_uint32_t,
    pub fn sections(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 20)));
    }
};
pub const struct_multiboot_tag_apm = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
    version: multiboot_uint16_t,
    cseg: multiboot_uint16_t,
    offset: multiboot_uint32_t,
    cseg_16: multiboot_uint16_t,
    dseg: multiboot_uint16_t,
    flags: multiboot_uint16_t,
    cseg_len: multiboot_uint16_t,
    cseg_16_len: multiboot_uint16_t,
    dseg_len: multiboot_uint16_t,
};
pub const struct_multiboot_tag_efi32 = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
    pointer: multiboot_uint32_t,
};
pub const struct_multiboot_tag_efi64 = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
    pointer: multiboot_uint64_t,
};
pub const struct_multiboot_tag_smbios = extern struct {
    type: multiboot_uint32_t align(4),
    size: multiboot_uint32_t,
    major: multiboot_uint8_t,
    minor: multiboot_uint8_t,
    reserved: [6]multiboot_uint8_t,
    pub fn tables(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint8_t) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint8_t);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 16)));
    }
};
pub const struct_multiboot_tag_old_acpi = extern struct {
    type: multiboot_uint32_t align(4),
    size: multiboot_uint32_t,
    pub fn rsdp(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint8_t) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint8_t);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 8)));
    }
};
pub const struct_multiboot_tag_new_acpi = extern struct {
    type: multiboot_uint32_t align(4),
    size: multiboot_uint32_t,
    pub fn rsdp(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint8_t) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint8_t);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 8)));
    }
};
pub const struct_multiboot_tag_network = extern struct {
    type: multiboot_uint32_t align(4),
    size: multiboot_uint32_t,
    pub fn dhcpack(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint8_t) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint8_t);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 8)));
    }
};
pub const struct_multiboot_tag_efi_mmap = extern struct {
    type: multiboot_uint32_t align(4),
    size: multiboot_uint32_t,
    descr_size: multiboot_uint32_t,
    descr_vers: multiboot_uint32_t,
    pub fn efi_mmap(self: anytype) @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint8_t) {
        const Intermediate = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), u8);
        const ReturnType = @import("std").zig.c_translation.FlexibleArrayType(@TypeOf(self), multiboot_uint8_t);
        return @as(ReturnType, @ptrCast(@alignCast(@as(Intermediate, @ptrCast(self)) + 16)));
    }
};
pub const struct_multiboot_tag_efi32_ih = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
    pointer: multiboot_uint32_t,
};
pub const struct_multiboot_tag_efi64_ih = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
    pointer: multiboot_uint64_t,
};
pub const struct_multiboot_tag_load_base_addr = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
    load_base_addr: multiboot_uint32_t,
};
pub const struct_multiboot_tag_kflags = extern struct {
    type: multiboot_uint32_t,
    size: multiboot_uint32_t,
    flags: multiboot_uint64_t,
};
pub const multiboot_tag_kflags_t = struct_multiboot_tag_kflags;
pub const struct_kmem_info = extern struct {
    cpu_id: u32,
    flags: u32,
    free_pages: u32,
    used_pages: u32,
    dma_buffer_count: u32,
    swap_page_count: u32,
};
pub const kmem_info_t = struct_kmem_info;
pub extern fn kmem_get_info(info_buffer: [*c]kmem_info_t, cpu_id: u32) c_int;
pub fn kmem_get_page_idx(arg_page_addr: usize) callconv(.C) usize {
    var page_addr = arg_page_addr;
    return page_addr >> @intCast(12);
}
pub fn kmem_get_page_addr(arg_page_idx: usize) callconv(.C) usize {
    var page_idx = arg_page_idx;
    return page_idx << @intCast(12);
}
pub fn kmem_get_page_base(arg_base: usize) callconv(.C) usize {
    var base = arg_base;
    return base & ~@as(c_ulonglong, 9223372036854779903);
}
pub fn kmem_set_page_base(arg_entry: [*c]pml_entry_t, arg_page_base: usize) callconv(.C) usize {
    var entry = arg_entry;
    var page_base = arg_page_base;
    entry.*.raw_bits &= @as(c_ulonglong, 9223372036854779903);
    entry.*.raw_bits |= kmem_get_page_base(page_base);
    return entry.*.raw_bits;
}
pub const PMRT_USABLE: c_int = 1;
pub const PMRT_RESERVED: c_int = 2;
pub const PMRT_KERNEL_RESERVED: c_int = 3;
pub const PMRT_ACPI_RECLAIM: c_int = 4;
pub const PMRT_ACPI_NVS: c_int = 5;
pub const PMRT_BAD_MEM: c_int = 6;
pub const PMRT_UNKNOWN: c_int = 7;
pub const PhysMemRangeType_t = c_uint;
pub const phys_mem_range_t = extern struct {
    type: PhysMemRangeType_t,
    start: u64,
    length: usize,
};
pub const contiguous_phys_virt_range_t = extern struct {
    upper: u64,
    lower: u64,
};
pub const struct_kmem_region = extern struct {
    m_page_count: usize,
    m_contiguous: @"bool",
    m_dir: [*c]page_dir_t,
    m_references: [*c]?*anyopaque,
};
pub const kregion_t = struct_kmem_region;
pub const kregion = [*c]struct_kmem_region;
pub const struct_kmem_page = extern struct {
    p0: usize,
    p1: usize,
    p2: usize,
    p3: usize,
    p4: usize,
    vbase: vaddr_t,
    pbase: paddr_t,
    dir: [*c]page_dir_t,
};
pub const struct_kmem_mapping = extern struct {
    m_size: usize,
    m_vbase: vaddr_t,
    m_pbase: paddr_t,
};
pub const kmapping_t = struct_kmem_mapping;
pub extern fn init_kmem_manager(mb_addr: [*c]usize) void;
pub extern fn protect_kernel(...) void;
pub extern fn protect_heap(...) void;
pub extern fn prep_mmap(mmap: [*c]struct_multiboot_tag_mmap) void;
pub extern fn parse_mmap(...) void;
pub extern fn kmem_load_page_dir(dir: usize, __disable_interrupts: @"bool") void;
pub extern fn kmem_from_phys(addr: usize, vbase: vaddr_t) vaddr_t;
pub extern fn kmem_from_dma(addr: paddr_t) vaddr_t;
pub extern fn kmem_ensure_high_mapping(addr: usize) vaddr_t;
pub extern fn kmem_to_phys(root: [*c]pml_entry_t, addr: usize) usize;
pub extern fn kmem_to_phys_aligned(root: [*c]pml_entry_t, addr: usize) usize;
pub extern fn kmem_set_phys_page_used(idx: usize) void;
pub extern fn kmem_set_phys_page_free(idx: usize) void;
pub extern fn kmem_set_phys_range_used(start_idx: usize, page_count: usize) void;
pub extern fn kmem_set_phys_range_free(start_idx: usize, page_count: usize) void;
pub extern fn kmem_set_phys_page(idx: usize, value: @"bool") void;
pub extern fn kmem_is_phys_page_used(idx: usize) @"bool";
pub extern fn kmem_invalidate_tlb_cache_entry(vaddr: usize) void;
pub extern fn kmem_invalidate_tlb_cache_range(vaddr: usize, size: usize) void;
pub extern fn kmem_refresh_tlb(...) void;
pub extern fn kmem_flush_tlb(...) void;
pub extern fn kmem_request_physical_page(...) ErrorOrPtr;
pub extern fn kmem_prepare_new_physical_page(...) ErrorOrPtr;
pub extern fn kmem_return_physical_page(page_base: paddr_t) ErrorOrPtr;
pub extern fn kmem_get_krnl_dir(...) [*c]pml_entry_t;
pub extern fn kmem_get_page(root: [*c]pml_entry_t, addr: usize, kmem_flags: u32, page_flags: u32) [*c]pml_entry_t;
pub extern fn kmem_get_page_with_quickmap(table: [*c]pml_entry_t, virt: vaddr_t, kmem_flags: u32, page_flags: u32) [*c]pml_entry_t;
pub extern fn kmem_clone_page(page: [*c]pml_entry_t) [*c]pml_entry_t;
pub extern fn kmem_set_page_flags(page: [*c]pml_entry_t, flags: u32) void;
pub extern fn kmem_map_page(table: [*c]pml_entry_t, virt: usize, phys: usize, kmem_flags: u32, page_flags: u32) @"bool";
pub extern fn kmem_map_range(table: [*c]pml_entry_t, virt_base: usize, phys_base: usize, page_count: usize, kmem_flags: u32, page_flags: u32) @"bool";
pub extern fn kmem_map_into(table: [*c]pml_entry_t, old: vaddr_t, new: vaddr_t, size: usize, kmem_flags: u32, page_flags: u32) ErrorOrPtr;
pub extern fn kmem_copy_kernel_mapping(new_table: [*c]pml_entry_t) ErrorOrPtr;
pub extern fn kmem_unmap_page(table: [*c]pml_entry_t, virt: usize) @"bool";
pub extern fn kmem_unmap_page_ex(table: [*c]pml_entry_t, virt: usize, custom_flags: u32) @"bool";
pub extern fn kmem_unmap_range(table: [*c]pml_entry_t, virt: usize, page_count: usize) @"bool";
pub extern fn kmem_unmap_range_ex(table: [*c]pml_entry_t, virt: usize, page_count: usize, custom_flags: u32) @"bool";
pub extern fn kmem_validate_ptr(process: ?*struct_proc, v_address: vaddr_t, size: usize) ErrorOrPtr;
pub extern fn kmem_ensure_mapped(table: [*c]pml_entry_t, base: vaddr_t, size: usize) ErrorOrPtr;
pub extern fn __kmem_kernel_alloc(addr: usize, size: usize, custom_flags: u32, page_flags: u32) ErrorOrPtr;
pub extern fn __kmem_kernel_alloc_range(size: usize, custom_flags: u32, page_flags: u32) ErrorOrPtr;
pub extern fn __kmem_dma_alloc(addr: usize, size: usize, custom_flag: u32, page_flags: u32) ErrorOrPtr;
pub extern fn __kmem_dma_alloc_range(size: usize, custom_flag: u32, page_flags: u32) ErrorOrPtr;
pub extern fn __kmem_alloc(map: [*c]pml_entry_t, resources: [*c]kresource_bundle_t, addr: paddr_t, size: usize, custom_flags: u32, page_flags: u32) ErrorOrPtr;
pub extern fn __kmem_alloc_ex(map: [*c]pml_entry_t, resources: [*c]kresource_bundle_t, addr: paddr_t, vbase: vaddr_t, size: usize, custom_flags: u32, page_flags: usize) ErrorOrPtr;
pub extern fn __kmem_alloc_range(map: [*c]pml_entry_t, resources: [*c]kresource_bundle_t, vbase: vaddr_t, size: usize, custom_flags: u32, page_flags: u32) ErrorOrPtr;
pub extern fn __kmem_dealloc(map: [*c]pml_entry_t, resources: [*c]kresource_bundle_t, virt_base: usize, size: usize) ErrorOrPtr;
pub extern fn __kmem_dealloc_unmap(map: [*c]pml_entry_t, resources: [*c]kresource_bundle_t, virt_base: usize, size: usize) ErrorOrPtr;
pub extern fn __kmem_dealloc_ex(map: [*c]pml_entry_t, resources: [*c]kresource_bundle_t, virt_base: usize, size: usize, unmap: @"bool", ignore_unused: @"bool", defer_res_release: @"bool") ErrorOrPtr;
pub extern fn __kmem_kernel_dealloc(virt_base: usize, size: usize) ErrorOrPtr;
pub extern fn __kmem_map_and_alloc_scattered(map: [*c]pml_entry_t, resources: [*c]kresource_bundle_t, vbase: vaddr_t, size: usize, custom_flags: u32, page_flags: u32) ErrorOrPtr;
pub extern fn kmem_user_alloc_range(p: ?*struct_proc, size: usize, custom_flags: u32, page_flags: u32) ErrorOrPtr;
pub extern fn kmem_user_alloc(p: ?*struct_proc, addr: paddr_t, size: usize, custom_flags: u32, page_flags: u32) ErrorOrPtr;
pub extern fn kmem_user_dealloc(p: ?*struct_proc, vaddr: vaddr_t, size: usize) ErrorOrPtr;
pub extern fn kmem_get_kernel_address(virtual_address: vaddr_t, from_map: [*c]pml_entry_t) ErrorOrPtr;
pub extern fn kmem_get_kernel_address_ex(virtual_address: vaddr_t, map_base: vaddr_t, from_map: [*c]pml_entry_t) ErrorOrPtr;
pub extern fn kmem_to_current_pagemap(vaddr: vaddr_t, external_map: [*c]pml_entry_t) ErrorOrPtr;
pub extern fn kmem_get_phys_ranges_list(...) [*c]const list_t;
pub extern fn kmem_create_page_dir(custom_flags: u32, initial_size: usize) page_dir_t;
pub extern fn kmem_destroy_page_dir(dir: [*c]pml_entry_t) ErrorOrPtr; // ./../src/aniva/dev/disk/shared.h:16:14: warning: struct demoted to opaque type - has bitfield
const struct_unnamed_22 = opaque {}; // ./../src/aniva/dev/disk/shared.h:141:12: warning: struct demoted to opaque type - has bitfield
pub const ata_identify_block_t = opaque {};
pub const disk_offset_t = usize;
pub const disk_uid_t = u8;
pub const struct_partitioned_disk_dev = extern struct {
    m_parent: [*c]struct_disk_dev,
    m_name: [*c]u8,
    m_start_lba: u64,
    m_end_lba: u64,
    m_flags: u32,
    m_block_size: u32,
    m_next: [*c]struct_partitioned_disk_dev,
};
pub const struct_gpt_partition_header = extern struct {
    sig: [2]u32 align(1),
    revision: u32 align(1),
    header_size: u32 align(1),
    crc32_header: u32 align(1),
    reserved: u32 align(1),
    current_lba: u64 align(1),
    backup_lba: u64 align(1),
    first_usable_lba: u64 align(1),
    last_usable_lba: u64 align(1),
    disk_guid1: [2]u64 align(1),
    partition_array_start_lba: u64 align(1),
    entries_count: u32 align(1),
    partition_entry_size: u32 align(1),
    crc32_entries_array: u32 align(1),
};
pub const gpt_partition_header_t = struct_gpt_partition_header;
pub const disk_dev_t = struct_disk_dev;
pub const struct_gpt_table = extern struct {
    m_header: gpt_partition_header_t,
    m_partition_count: usize,
    m_device: [*c]disk_dev_t,
    m_partitions: [*c]list_t,
};
pub const struct_mbr_table = extern struct {
    header_copy: mbr_header_t,
    entry_count: u8,
    partitions: [*c]list_t,
};
const union_unnamed_23 = extern union {
    gpt_table: [*c]struct_gpt_table,
    mbr_table: [*c]struct_mbr_table,
};
pub const struct_disk_dev = extern struct {
    m_device_name: [*c]const u8,
    m_flags: u32,
    m_uid: disk_uid_t,
    m_partition_type: u8,
    m_firmware_rev: [4]u16,
    m_max_blk: usize,
    m_ops: [*c]struct_device_disk_endpoint,
    m_logical_sector_size: u32,
    m_physical_sector_size: u32,
    m_effective_sector_size: u32,
    m_max_sector_transfer_count: u16,
    m_partitioned_dev_count: usize,
    m_devs: [*c]struct_partitioned_disk_dev,
    unnamed_0: union_unnamed_23,
    m_dev: [*c]struct_device,
    m_parent: ?*anyopaque,
};
pub const mbr_entry_t = extern struct {
    status: u8 align(1),
    chs1: [3]u8 align(1),
    type: u8 align(1),
    chs2: [3]u8 align(1),
    offset: u32 align(1),
    length: u32 align(1),
};
pub const mbr_partition_names: [*c][*c]const u8 = @extern([*c][*c]const u8, .{
    .name = "mbr_partition_names",
});
const struct_unnamed_24 = extern struct {
    bootstrap_code: [446]u8,
    entries: [4]mbr_entry_t,
    signature: u16,
};
const struct_unnamed_25 = extern struct {
    bootstrap_code_1: [218]u8,
    zero: u16,
    drive: u8,
    seconds: u8,
    minutes: u8,
    hours: u8,
    bootstrap_code_2: [216]u8,
    disk_signature: u32,
    disk_signature_end: u16,
    entries: [4]mbr_entry_t,
    signature: u16,
};
pub const mbr_header_t = extern union {
    legacy: struct_unnamed_24 align(1),
    modern: struct_unnamed_25 align(1),
};
pub const mbr_table_t = struct_mbr_table;
pub extern fn create_mbr_table(device: [*c]struct_disk_dev, start_lba: usize) [*c]mbr_table_t;
pub extern fn destroy_mbr_table(table: [*c]mbr_table_t) void;
pub const partitioned_disk_dev_t = struct_partitioned_disk_dev;
pub fn get_blockcount(arg_device_1: [*c]disk_dev_t, arg_size: usize) callconv(.C) usize {
    var device_1 = arg_device_1;
    var size = arg_size;
    return (if ((size % @as(usize, @bitCast(@as(c_ulonglong, device_1.*.m_logical_sector_size)))) == @as(usize, @bitCast(@as(c_longlong, @as(c_int, 0))))) size else (size +% @as(usize, @bitCast(@as(c_ulonglong, device_1.*.m_logical_sector_size)))) -% (size % @as(usize, @bitCast(@as(c_ulonglong, device_1.*.m_logical_sector_size))))) / @as(usize, @bitCast(@as(c_ulonglong, device_1.*.m_logical_sector_size)));
}
pub extern fn create_generic_disk(parent: [*c]struct_aniva_driver, name: [*c]u8, private: ?*anyopaque, eps: [*c]struct_device_endpoint, ep_count: u32) [*c]disk_dev_t;
pub extern fn destroy_generic_disk(device: [*c]disk_dev_t) void;
pub extern fn disk_set_effective_sector_count(dev: [*c]disk_dev_t, count: u32) void;
pub extern fn read_sync_partitioned(dev: [*c]partitioned_disk_dev_t, buffer: ?*anyopaque, size: usize, offset: disk_offset_t) c_int;
pub extern fn write_sync_partitioned(dev: [*c]partitioned_disk_dev_t, buffer: ?*anyopaque, size: usize, offset: disk_offset_t) c_int;
pub extern fn read_sync_partitioned_blocks(dev: [*c]partitioned_disk_dev_t, buffer: ?*anyopaque, count: usize, block: usize) c_int;
pub extern fn write_sync_partitioned_blocks(dev: [*c]partitioned_disk_dev_t, buffer: ?*anyopaque, count: usize, block: usize) c_int;
pub extern fn pd_bread(dev: [*c]partitioned_disk_dev_t, buffer: ?*anyopaque, blockn: usize) c_int;
pub extern fn pd_bwrite(dev: [*c]partitioned_disk_dev_t, buffer: ?*anyopaque, blockn: usize) c_int;
pub extern fn pd_set_blocksize(dev: [*c]partitioned_disk_dev_t, blksize: u32) c_int;
pub extern fn init_gdisk_dev(...) void;
pub extern fn init_root_device_probing(...) void;
pub extern fn init_root_ramdev(...) void;
pub extern fn register_gdisk_dev(device: [*c]disk_dev_t) ErrorOrPtr;
pub extern fn register_gdisk_dev_with_uid(device: [*c]disk_dev_t, uid: disk_uid_t) ErrorOrPtr;
pub extern fn unregister_gdisk_dev(device: [*c]disk_dev_t) ErrorOrPtr;
pub extern fn gdisk_is_valid(device: [*c]disk_dev_t) @"bool";
pub extern fn diskdev_populate_partition_table(dev: [*c]disk_dev_t) c_int;
pub extern fn find_partition_at(diskdev: [*c]disk_dev_t, idx: u32) [*c]partitioned_disk_dev_t;
pub extern fn find_gdisk_device(uid: disk_uid_t) [*c]disk_dev_t;
pub extern fn create_partitioned_disk_dev(parent: [*c]disk_dev_t, name: [*c]u8, start: u64, end: u64, flags: u32) [*c]partitioned_disk_dev_t;
pub extern fn destroy_partitioned_disk_dev(dev: [*c]partitioned_disk_dev_t) void;
pub extern fn attach_partitioned_disk_device(generic: [*c]disk_dev_t, dev: [*c]partitioned_disk_dev_t) void;
pub extern fn detach_partitioned_disk_device(generic: [*c]disk_dev_t, dev: [*c]partitioned_disk_dev_t) void;
pub extern fn fetch_designated_device_for_block(generic: [*c]disk_dev_t, block: usize) [*c][*c]partitioned_disk_dev_t;
pub extern fn generic_disk_opperation(dev: [*c]disk_dev_t, buffer: ?*anyopaque, size: usize, offset: disk_offset_t) c_int;
pub const struct_gpt_partition_type = extern struct {
    m_guid: [16]u8,
    m_name: [72]u8,
};
pub const gpt_partition_type_t = struct_gpt_partition_type;
pub inline fn gpt_part_type_decode_name(arg_type: [*c]gpt_partition_type_t) void {
    var @"type" = arg_type;
    var index: usize = 0;
    var decoded_name: [72]u8 = [1]u8{
        0,
    } ++ [1]u8{0} ** 71;
    {
        var i: usize = 0;
        while (i < @as(usize, @bitCast(@as(c_longlong, @as(c_int, 72))))) : (i +%= 1) {
            if (@"type".*.m_name[@as(usize, @intCast(i))] != 0) {
                decoded_name[@as(usize, @intCast(index))] = @"type".*.m_name[@as(usize, @intCast(i))];
                index +%= 1;
            }
        }
    }
    _ = memcpy(@as(?*anyopaque, @ptrCast(@as([*c]u8, @ptrCast(@alignCast(&@"type".*.m_name))))), @as(?*const anyopaque, @ptrCast(@as([*c]u8, @ptrCast(@alignCast(&decoded_name))))), @as(usize, @bitCast(@as(c_longlong, @as(c_int, 72)))));
}
pub const struct_gpt_partition = extern struct {
    m_type: gpt_partition_type_t,
    m_path: [*c]u8,
    m_index: usize,
    m_start_lba: u64,
    m_end_lba: u64,
};
pub const gpt_partition_t = struct_gpt_partition;
pub const struct_gpt_partition_entry = extern struct {
    partition_guid: [16]u8 align(1),
    unique_guid: [16]u8 align(1),
    first_lba: u64 align(1),
    last_lba: u64 align(1),
    attributes: u64 align(1),
    partition_name: [36]u16 align(1),
};
pub const gpt_partition_entry_t = struct_gpt_partition_entry;
pub const gpt_table_t = struct_gpt_table;
pub extern fn create_gpt_table(device: [*c]disk_dev_t) [*c]gpt_table_t;
pub extern fn destroy_gpt_table(table: [*c]gpt_table_t) void;
pub extern fn create_gpt_partition(entry: [*c]gpt_partition_entry_t, index: usize) [*c]gpt_partition_t;
pub extern fn destroy_gpt_partition(partition: [*c]gpt_partition_t) void;
pub extern fn disk_try_copy_gpt_header(device: [*c]disk_dev_t, table: [*c]gpt_table_t) @"bool";
pub const __INTMAX_C_SUFFIX__ = @compileError("unable to translate macro: undefined identifier `L`"); // (no file):80:9
pub const __UINTMAX_C_SUFFIX__ = @compileError("unable to translate macro: undefined identifier `UL`"); // (no file):86:9
pub const __FLT16_DENORM_MIN__ = @compileError("unable to translate C expr: unexpected token 'IntegerLiteral'"); // (no file):109:9
pub const __FLT16_EPSILON__ = @compileError("unable to translate C expr: unexpected token 'IntegerLiteral'"); // (no file):113:9
pub const __FLT16_MAX__ = @compileError("unable to translate C expr: unexpected token 'IntegerLiteral'"); // (no file):119:9
pub const __FLT16_MIN__ = @compileError("unable to translate C expr: unexpected token 'IntegerLiteral'"); // (no file):122:9
pub const __INT64_C_SUFFIX__ = @compileError("unable to translate macro: undefined identifier `L`"); // (no file):183:9
pub const __UINT32_C_SUFFIX__ = @compileError("unable to translate macro: undefined identifier `U`"); // (no file):205:9
pub const __UINT64_C_SUFFIX__ = @compileError("unable to translate macro: undefined identifier `UL`"); // (no file):213:9
pub const __seg_gs = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // (no file):343:9
pub const __seg_fs = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // (no file):344:9
pub const @"asm" = @compileError("unable to translate macro: undefined identifier `__asm__`"); // ./../src/aniva/libk/stddef.h:40:9
pub const ALWAYS_INLINE = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/libk/stddef.h:46:9
pub const NOINLINE = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/libk/stddef.h:47:9
pub const NORETURN = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/libk/stddef.h:48:9
pub const NAKED = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/libk/stddef.h:49:9
pub const SECTION = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/libk/stddef.h:50:9
pub const ALIGN = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/libk/stddef.h:51:9
pub const USED = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/libk/stddef.h:52:9
pub const UNUSED = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/libk/stddef.h:53:9
pub const __init = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/libk/stddef.h:54:9
pub const __mmio = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/libk/stddef.h:56:9
pub const DYNAMIC_CAST = @compileError("unable to translate C expr: unexpected token ')'"); // ./../src/aniva/libk/stddef.h:59:9
pub const va_start = @compileError("unable to translate macro: undefined identifier `__builtin_va_start`"); // ./../src/aniva/libk/stddef.h:61:9
pub const va_end = @compileError("unable to translate macro: undefined identifier `__builtin_va_end`"); // ./../src/aniva/libk/stddef.h:64:9
pub const va_arg = @compileError("unable to translate macro: undefined identifier `__builtin_va_arg`"); // ./../src/aniva/libk/stddef.h:67:9
pub const arrlen = @compileError("unable to translate C expr: unexpected token '*'"); // ./../src/aniva/libk/stddef.h:70:9
pub const ASSERT = @compileError("unable to translate macro: undefined identifier `__FILE__`"); // ./../src/aniva/libk/flow/error.h:124:9
pub const ASSERT_MSG = @compileError("unable to translate macro: undefined identifier `__FILE__`"); // ./../src/aniva/libk/flow/error.h:125:9
pub const TRY = @compileError("unable to translate macro: undefined identifier `_status`"); // ./../src/aniva/libk/flow/error.h:191:9
pub const ITTERATE = @compileError("unable to translate macro: undefined identifier `itterator`"); // ./../src/aniva/libk/data/linkedlist.h:20:9
pub const SKIP_ITTERATION = @compileError("unable to translate macro: undefined identifier `itterator`"); // ./../src/aniva/libk/data/linkedlist.h:24:9
pub const ENDITTERATE = @compileError("unable to translate macro: undefined identifier `itterator`"); // ./../src/aniva/libk/data/linkedlist.h:27:9
pub const FOREACH = @compileError("unable to translate C expr: unexpected token 'for'"); // ./../src/aniva/libk/data/linkedlist.h:30:9
pub const EXPORT_DRIVER_PTR = @compileError("unable to translate macro: undefined identifier `exported_`"); // ./../src/aniva/dev/core.h:86:9
pub const EXPORT_CORE_DRIVER = @compileError("unable to translate macro: undefined identifier `exported_core_`"); // ./../src/aniva/dev/core.h:87:9
pub const EXPORT_DRIVER = @compileError("unable to translate C expr: unexpected token 'static'"); // ./../src/aniva/dev/core.h:88:9
pub const EXPORT_DEPENDENCIES = @compileError("unable to translate C expr: unexpected token 'static'"); // ./../src/aniva/dev/core.h:89:9
pub const DRIVER_VERSION = @compileError("unable to translate C expr: unexpected token '{'"); // ./../src/aniva/dev/core.h:215:9
pub const FOREACH_VEC = @compileError("unable to translate C expr: unexpected token 'for'"); // ./../src/aniva/libk/data/vector.h:19:9
pub const ACPI_INLINE = @compileError("unable to translate macro: undefined identifier `__inline__`"); // ./../src/aniva/system/acpi/acpica/platform/acgcc.h:155:9
pub const ACPI_GET_FUNCTION_NAME = @compileError("unable to translate macro: undefined identifier `__func__`"); // ./../src/aniva/system/acpi/acpica/platform/acgcc.h:159:9
pub const ACPI_PRINTF_LIKE = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/system/acpi/acpica/platform/acgcc.h:165:9
pub const ACPI_UNUSED_VAR = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/system/acpi/acpica/platform/acgcc.h:173:9
pub const ACPI_FALLTHROUGH = @compileError("unable to translate macro: undefined identifier `__attribute__`"); // ./../src/aniva/system/acpi/acpica/platform/acgcc.h:195:9
pub const ACPI_FLEX_ARRAY = @compileError("unable to translate macro: undefined identifier `__Empty_`"); // ./../src/aniva/system/acpi/acpica/platform/acgcc.h:204:9
pub const FOREACH_ZONESTORE = @compileError("unable to translate C expr: unexpected token 'for'"); // ./../src/aniva/mem/zalloc.h:117:9
pub const GET_OFFSET = @compileError("unable to translate macro: undefined identifier `__builtin_offsetof`"); // ./../src/aniva/system/asm_specifics.h:5:9
pub const ACPI_DEBUG_DEFAULT = @compileError("unable to translate macro: undefined identifier `ACPI_LV_REPAIR`"); // ./../src/aniva/system/acpi/acpica/platform/acaniva.h:44:9
pub const ACPI_EXPORT_SYMBOL = @compileError("unable to translate C expr: unexpected token 'Eof'"); // ./../src/aniva/system/acpi/acpica/platform/acaniva.h:52:9
pub const ACPI_SPINLOCK = @compileError("unable to translate C expr: unexpected token 'Eof'"); // ./../src/aniva/system/acpi/acpica/platform/acaniva.h:56:9
pub const ACPI_STRUCT_INIT = @compileError("unable to translate C expr: unexpected token '.'"); // ./../src/aniva/system/acpi/acpica/platform/acaniva.h:72:9
pub const ACPI_ACQUIRE_GLOBAL_LOCK = @compileError("unable to translate C expr: unexpected token '='"); // ./../src/aniva/system/acpi/acpica/platform/acenv.h:330:9
pub const ACPI_RELEASE_GLOBAL_LOCK = @compileError("unable to translate C expr: unexpected token '='"); // ./../src/aniva/system/acpi/acpica/platform/acenv.h:334:9
pub const ACPI_FLUSH_CPU_CACHE = @compileError("unable to translate C expr: unexpected token 'Eof'"); // ./../src/aniva/system/acpi/acpica/platform/acenv.h:340:9
pub const ACPI_EXPORT_SYMBOL_INIT = @compileError("unable to translate C expr: unexpected token 'Eof'"); // ./../src/aniva/system/acpi/acpica/actypes.h:440:9
pub const ACPI_DEBUG_INITIALIZE = @compileError("unable to translate C expr: unexpected token 'Eof'"); // ./../src/aniva/system/acpi/acpica/actypes.h:452:9
pub const ACPI_ALLOCATE = @compileError("unable to translate macro: undefined identifier `AcpiOsAllocate`"); // ./../src/aniva/system/acpi/acpica/actypes.h:485:9
pub const ACPI_ALLOCATE_ZEROED = @compileError("unable to translate macro: undefined identifier `AcpiOsAllocateZeroed`"); // ./../src/aniva/system/acpi/acpica/actypes.h:486:9
pub const ACPI_FREE = @compileError("unable to translate macro: undefined identifier `AcpiOsFree`"); // ./../src/aniva/system/acpi/acpica/actypes.h:487:9
pub const ACPI_MEM_TRACKING = @compileError("unable to translate C expr: unexpected token 'Eof'"); // ./../src/aniva/system/acpi/acpica/actypes.h:488:9
pub const ACPI_SET_BIT = @compileError("unable to translate C expr: expected ')' instead got '|='"); // ./../src/aniva/system/acpi/acpica/actypes.h:634:9
pub const ACPI_CLEAR_BIT = @compileError("unable to translate C expr: expected ')' instead got '&='"); // ./../src/aniva/system/acpi/acpica/actypes.h:635:9
pub const ACPI_ARRAY_LENGTH = @compileError("unable to translate C expr: unexpected token '('"); // ./../src/aniva/system/acpi/acpica/actypes.h:641:9
pub const ACPI_CAST_PTR = @compileError("unable to translate C expr: unexpected token ')'"); // ./../src/aniva/system/acpi/acpica/actypes.h:645:9
pub const ACPI_CAST_INDIRECT_PTR = @compileError("unable to translate C expr: unexpected token ')'"); // ./../src/aniva/system/acpi/acpica/actypes.h:646:9
pub const ACPI_COPY_NAMESEG = @compileError("unable to translate C expr: expected ')' instead got '='"); // ./../src/aniva/system/acpi/acpica/actypes.h:666:9
pub const ACPI_IS_OEM_SIG = @compileError("unable to translate macro: undefined identifier `strnlen`"); // ./../src/aniva/system/acpi/acpica/actypes.h:678:9
pub const ACPI_MPST_CHANNEL_INFO = @compileError("unable to translate macro: undefined identifier `ChannelId`"); // ./../src/aniva/system/acpi/acpica/actbl2.h:1855:9
pub const ACPI_FADT_V1_SIZE = @compileError("unable to translate macro: undefined identifier `Flags`"); // ./../src/aniva/system/acpi/acpica/actbl.h:572:9
pub const ACPI_FADT_V2_SIZE = @compileError("unable to translate macro: undefined identifier `MinorRevision`"); // ./../src/aniva/system/acpi/acpica/actbl.h:573:9
pub const ACPI_FADT_V3_SIZE = @compileError("unable to translate macro: undefined identifier `SleepControl`"); // ./../src/aniva/system/acpi/acpica/actbl.h:574:9
pub const ACPI_FADT_V5_SIZE = @compileError("unable to translate macro: undefined identifier `HypervisorId`"); // ./../src/aniva/system/acpi/acpica/actbl.h:575:9
pub const DRV_DEP = @compileError("unable to translate C expr: unexpected token '{'"); // ./../src/aniva/dev/driver.h:47:9
pub const DRV_DEP_END = @compileError("unable to translate C expr: unexpected token '{'"); // ./../src/aniva/dev/driver.h:48:9
pub const EARLY_KERNEL_HEAP_BASE = @compileError("unable to translate macro: undefined identifier `_kernel_end`"); // ./../src/aniva/mem/kmem_manager.h:48:9
pub const __llvm__ = @as(c_int, 1);
pub const __clang__ = @as(c_int, 1);
pub const __clang_major__ = @as(c_int, 16);
pub const __clang_minor__ = @as(c_int, 0);
pub const __clang_patchlevel__ = @as(c_int, 6);
pub const __clang_version__ = "16.0.6 ";
pub const __GNUC__ = @as(c_int, 4);
pub const __GNUC_MINOR__ = @as(c_int, 2);
pub const __GNUC_PATCHLEVEL__ = @as(c_int, 1);
pub const __GXX_ABI_VERSION = @as(c_int, 1002);
pub const __ATOMIC_RELAXED = @as(c_int, 0);
pub const __ATOMIC_CONSUME = @as(c_int, 1);
pub const __ATOMIC_ACQUIRE = @as(c_int, 2);
pub const __ATOMIC_RELEASE = @as(c_int, 3);
pub const __ATOMIC_ACQ_REL = @as(c_int, 4);
pub const __ATOMIC_SEQ_CST = @as(c_int, 5);
pub const __OPENCL_MEMORY_SCOPE_WORK_ITEM = @as(c_int, 0);
pub const __OPENCL_MEMORY_SCOPE_WORK_GROUP = @as(c_int, 1);
pub const __OPENCL_MEMORY_SCOPE_DEVICE = @as(c_int, 2);
pub const __OPENCL_MEMORY_SCOPE_ALL_SVM_DEVICES = @as(c_int, 3);
pub const __OPENCL_MEMORY_SCOPE_SUB_GROUP = @as(c_int, 4);
pub const __PRAGMA_REDEFINE_EXTNAME = @as(c_int, 1);
pub const __VERSION__ = "Clang 16.0.6";
pub const __OBJC_BOOL_IS_BOOL = @as(c_int, 0);
pub const __CONSTANT_CFSTRINGS__ = @as(c_int, 1);
pub const __clang_literal_encoding__ = "UTF-8";
pub const __clang_wide_literal_encoding__ = "UTF-32";
pub const __ORDER_LITTLE_ENDIAN__ = @as(c_int, 1234);
pub const __ORDER_BIG_ENDIAN__ = @as(c_int, 4321);
pub const __ORDER_PDP_ENDIAN__ = @as(c_int, 3412);
pub const __BYTE_ORDER__ = __ORDER_LITTLE_ENDIAN__;
pub const __LITTLE_ENDIAN__ = @as(c_int, 1);
pub const _LP64 = @as(c_int, 1);
pub const __LP64__ = @as(c_int, 1);
pub const __CHAR_BIT__ = @as(c_int, 8);
pub const __BOOL_WIDTH__ = @as(c_int, 8);
pub const __SHRT_WIDTH__ = @as(c_int, 16);
pub const __INT_WIDTH__ = @as(c_int, 32);
pub const __LONG_WIDTH__ = @as(c_int, 64);
pub const __LLONG_WIDTH__ = @as(c_int, 64);
pub const __BITINT_MAXWIDTH__ = @import("std").zig.c_translation.promoteIntLiteral(c_int, 8388608, .decimal);
pub const __SCHAR_MAX__ = @as(c_int, 127);
pub const __SHRT_MAX__ = @as(c_int, 32767);
pub const __INT_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __LONG_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __LONG_LONG_MAX__ = @as(c_longlong, 9223372036854775807);
pub const __WCHAR_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __WCHAR_WIDTH__ = @as(c_int, 32);
pub const __WINT_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_uint, 4294967295, .decimal);
pub const __WINT_WIDTH__ = @as(c_int, 32);
pub const __INTMAX_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __INTMAX_WIDTH__ = @as(c_int, 64);
pub const __SIZE_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const __SIZE_WIDTH__ = @as(c_int, 64);
pub const __UINTMAX_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const __UINTMAX_WIDTH__ = @as(c_int, 64);
pub const __PTRDIFF_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __PTRDIFF_WIDTH__ = @as(c_int, 64);
pub const __INTPTR_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __INTPTR_WIDTH__ = @as(c_int, 64);
pub const __UINTPTR_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const __UINTPTR_WIDTH__ = @as(c_int, 64);
pub const __SIZEOF_DOUBLE__ = @as(c_int, 8);
pub const __SIZEOF_FLOAT__ = @as(c_int, 4);
pub const __SIZEOF_INT__ = @as(c_int, 4);
pub const __SIZEOF_LONG__ = @as(c_int, 8);
pub const __SIZEOF_LONG_DOUBLE__ = @as(c_int, 16);
pub const __SIZEOF_LONG_LONG__ = @as(c_int, 8);
pub const __SIZEOF_POINTER__ = @as(c_int, 8);
pub const __SIZEOF_SHORT__ = @as(c_int, 2);
pub const __SIZEOF_PTRDIFF_T__ = @as(c_int, 8);
pub const __SIZEOF_SIZE_T__ = @as(c_int, 8);
pub const __SIZEOF_WCHAR_T__ = @as(c_int, 4);
pub const __SIZEOF_WINT_T__ = @as(c_int, 4);
pub const __SIZEOF_INT128__ = @as(c_int, 16);
pub const __INTMAX_TYPE__ = c_long;
pub const __INTMAX_FMTd__ = "ld";
pub const __INTMAX_FMTi__ = "li";
pub const __UINTMAX_TYPE__ = c_ulong;
pub const __UINTMAX_FMTo__ = "lo";
pub const __UINTMAX_FMTu__ = "lu";
pub const __UINTMAX_FMTx__ = "lx";
pub const __UINTMAX_FMTX__ = "lX";
pub const __PTRDIFF_TYPE__ = c_long;
pub const __PTRDIFF_FMTd__ = "ld";
pub const __PTRDIFF_FMTi__ = "li";
pub const __INTPTR_TYPE__ = c_long;
pub const __INTPTR_FMTd__ = "ld";
pub const __INTPTR_FMTi__ = "li";
pub const __SIZE_TYPE__ = c_ulong;
pub const __SIZE_FMTo__ = "lo";
pub const __SIZE_FMTu__ = "lu";
pub const __SIZE_FMTx__ = "lx";
pub const __SIZE_FMTX__ = "lX";
pub const __WCHAR_TYPE__ = c_int;
pub const __WINT_TYPE__ = c_uint;
pub const __SIG_ATOMIC_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __SIG_ATOMIC_WIDTH__ = @as(c_int, 32);
pub const __CHAR16_TYPE__ = c_ushort;
pub const __CHAR32_TYPE__ = c_uint;
pub const __UINTPTR_TYPE__ = c_ulong;
pub const __UINTPTR_FMTo__ = "lo";
pub const __UINTPTR_FMTu__ = "lu";
pub const __UINTPTR_FMTx__ = "lx";
pub const __UINTPTR_FMTX__ = "lX";
pub const __FLT16_HAS_DENORM__ = @as(c_int, 1);
pub const __FLT16_DIG__ = @as(c_int, 3);
pub const __FLT16_DECIMAL_DIG__ = @as(c_int, 5);
pub const __FLT16_HAS_INFINITY__ = @as(c_int, 1);
pub const __FLT16_HAS_QUIET_NAN__ = @as(c_int, 1);
pub const __FLT16_MANT_DIG__ = @as(c_int, 11);
pub const __FLT16_MAX_10_EXP__ = @as(c_int, 4);
pub const __FLT16_MAX_EXP__ = @as(c_int, 16);
pub const __FLT16_MIN_10_EXP__ = -@as(c_int, 4);
pub const __FLT16_MIN_EXP__ = -@as(c_int, 13);
pub const __FLT_DENORM_MIN__ = @as(f32, 1.40129846e-45);
pub const __FLT_HAS_DENORM__ = @as(c_int, 1);
pub const __FLT_DIG__ = @as(c_int, 6);
pub const __FLT_DECIMAL_DIG__ = @as(c_int, 9);
pub const __FLT_EPSILON__ = @as(f32, 1.19209290e-7);
pub const __FLT_HAS_INFINITY__ = @as(c_int, 1);
pub const __FLT_HAS_QUIET_NAN__ = @as(c_int, 1);
pub const __FLT_MANT_DIG__ = @as(c_int, 24);
pub const __FLT_MAX_10_EXP__ = @as(c_int, 38);
pub const __FLT_MAX_EXP__ = @as(c_int, 128);
pub const __FLT_MAX__ = @as(f32, 3.40282347e+38);
pub const __FLT_MIN_10_EXP__ = -@as(c_int, 37);
pub const __FLT_MIN_EXP__ = -@as(c_int, 125);
pub const __FLT_MIN__ = @as(f32, 1.17549435e-38);
pub const __DBL_DENORM_MIN__ = @as(f64, 4.9406564584124654e-324);
pub const __DBL_HAS_DENORM__ = @as(c_int, 1);
pub const __DBL_DIG__ = @as(c_int, 15);
pub const __DBL_DECIMAL_DIG__ = @as(c_int, 17);
pub const __DBL_EPSILON__ = @as(f64, 2.2204460492503131e-16);
pub const __DBL_HAS_INFINITY__ = @as(c_int, 1);
pub const __DBL_HAS_QUIET_NAN__ = @as(c_int, 1);
pub const __DBL_MANT_DIG__ = @as(c_int, 53);
pub const __DBL_MAX_10_EXP__ = @as(c_int, 308);
pub const __DBL_MAX_EXP__ = @as(c_int, 1024);
pub const __DBL_MAX__ = @as(f64, 1.7976931348623157e+308);
pub const __DBL_MIN_10_EXP__ = -@as(c_int, 307);
pub const __DBL_MIN_EXP__ = -@as(c_int, 1021);
pub const __DBL_MIN__ = @as(f64, 2.2250738585072014e-308);
pub const __LDBL_DENORM_MIN__ = @as(c_longdouble, 3.64519953188247460253e-4951);
pub const __LDBL_HAS_DENORM__ = @as(c_int, 1);
pub const __LDBL_DIG__ = @as(c_int, 18);
pub const __LDBL_DECIMAL_DIG__ = @as(c_int, 21);
pub const __LDBL_EPSILON__ = @as(c_longdouble, 1.08420217248550443401e-19);
pub const __LDBL_HAS_INFINITY__ = @as(c_int, 1);
pub const __LDBL_HAS_QUIET_NAN__ = @as(c_int, 1);
pub const __LDBL_MANT_DIG__ = @as(c_int, 64);
pub const __LDBL_MAX_10_EXP__ = @as(c_int, 4932);
pub const __LDBL_MAX_EXP__ = @as(c_int, 16384);
pub const __LDBL_MAX__ = @as(c_longdouble, 1.18973149535723176502e+4932);
pub const __LDBL_MIN_10_EXP__ = -@as(c_int, 4931);
pub const __LDBL_MIN_EXP__ = -@as(c_int, 16381);
pub const __LDBL_MIN__ = @as(c_longdouble, 3.36210314311209350626e-4932);
pub const __POINTER_WIDTH__ = @as(c_int, 64);
pub const __BIGGEST_ALIGNMENT__ = @as(c_int, 16);
pub const __WINT_UNSIGNED__ = @as(c_int, 1);
pub const __INT8_TYPE__ = i8;
pub const __INT8_FMTd__ = "hhd";
pub const __INT8_FMTi__ = "hhi";
pub const __INT8_C_SUFFIX__ = "";
pub const __INT16_TYPE__ = c_short;
pub const __INT16_FMTd__ = "hd";
pub const __INT16_FMTi__ = "hi";
pub const __INT16_C_SUFFIX__ = "";
pub const __INT32_TYPE__ = c_int;
pub const __INT32_FMTd__ = "d";
pub const __INT32_FMTi__ = "i";
pub const __INT32_C_SUFFIX__ = "";
pub const __INT64_TYPE__ = c_long;
pub const __INT64_FMTd__ = "ld";
pub const __INT64_FMTi__ = "li";
pub const __UINT8_TYPE__ = u8;
pub const __UINT8_FMTo__ = "hho";
pub const __UINT8_FMTu__ = "hhu";
pub const __UINT8_FMTx__ = "hhx";
pub const __UINT8_FMTX__ = "hhX";
pub const __UINT8_C_SUFFIX__ = "";
pub const __UINT8_MAX__ = @as(c_int, 255);
pub const __INT8_MAX__ = @as(c_int, 127);
pub const __UINT16_TYPE__ = c_ushort;
pub const __UINT16_FMTo__ = "ho";
pub const __UINT16_FMTu__ = "hu";
pub const __UINT16_FMTx__ = "hx";
pub const __UINT16_FMTX__ = "hX";
pub const __UINT16_C_SUFFIX__ = "";
pub const __UINT16_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_int, 65535, .decimal);
pub const __INT16_MAX__ = @as(c_int, 32767);
pub const __UINT32_TYPE__ = c_uint;
pub const __UINT32_FMTo__ = "o";
pub const __UINT32_FMTu__ = "u";
pub const __UINT32_FMTx__ = "x";
pub const __UINT32_FMTX__ = "X";
pub const __UINT32_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_uint, 4294967295, .decimal);
pub const __INT32_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __UINT64_TYPE__ = c_ulong;
pub const __UINT64_FMTo__ = "lo";
pub const __UINT64_FMTu__ = "lu";
pub const __UINT64_FMTx__ = "lx";
pub const __UINT64_FMTX__ = "lX";
pub const __UINT64_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const __INT64_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __INT_LEAST8_TYPE__ = i8;
pub const __INT_LEAST8_MAX__ = @as(c_int, 127);
pub const __INT_LEAST8_WIDTH__ = @as(c_int, 8);
pub const __INT_LEAST8_FMTd__ = "hhd";
pub const __INT_LEAST8_FMTi__ = "hhi";
pub const __UINT_LEAST8_TYPE__ = u8;
pub const __UINT_LEAST8_MAX__ = @as(c_int, 255);
pub const __UINT_LEAST8_FMTo__ = "hho";
pub const __UINT_LEAST8_FMTu__ = "hhu";
pub const __UINT_LEAST8_FMTx__ = "hhx";
pub const __UINT_LEAST8_FMTX__ = "hhX";
pub const __INT_LEAST16_TYPE__ = c_short;
pub const __INT_LEAST16_MAX__ = @as(c_int, 32767);
pub const __INT_LEAST16_WIDTH__ = @as(c_int, 16);
pub const __INT_LEAST16_FMTd__ = "hd";
pub const __INT_LEAST16_FMTi__ = "hi";
pub const __UINT_LEAST16_TYPE__ = c_ushort;
pub const __UINT_LEAST16_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_int, 65535, .decimal);
pub const __UINT_LEAST16_FMTo__ = "ho";
pub const __UINT_LEAST16_FMTu__ = "hu";
pub const __UINT_LEAST16_FMTx__ = "hx";
pub const __UINT_LEAST16_FMTX__ = "hX";
pub const __INT_LEAST32_TYPE__ = c_int;
pub const __INT_LEAST32_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __INT_LEAST32_WIDTH__ = @as(c_int, 32);
pub const __INT_LEAST32_FMTd__ = "d";
pub const __INT_LEAST32_FMTi__ = "i";
pub const __UINT_LEAST32_TYPE__ = c_uint;
pub const __UINT_LEAST32_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_uint, 4294967295, .decimal);
pub const __UINT_LEAST32_FMTo__ = "o";
pub const __UINT_LEAST32_FMTu__ = "u";
pub const __UINT_LEAST32_FMTx__ = "x";
pub const __UINT_LEAST32_FMTX__ = "X";
pub const __INT_LEAST64_TYPE__ = c_long;
pub const __INT_LEAST64_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __INT_LEAST64_WIDTH__ = @as(c_int, 64);
pub const __INT_LEAST64_FMTd__ = "ld";
pub const __INT_LEAST64_FMTi__ = "li";
pub const __UINT_LEAST64_TYPE__ = c_ulong;
pub const __UINT_LEAST64_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const __UINT_LEAST64_FMTo__ = "lo";
pub const __UINT_LEAST64_FMTu__ = "lu";
pub const __UINT_LEAST64_FMTx__ = "lx";
pub const __UINT_LEAST64_FMTX__ = "lX";
pub const __INT_FAST8_TYPE__ = i8;
pub const __INT_FAST8_MAX__ = @as(c_int, 127);
pub const __INT_FAST8_WIDTH__ = @as(c_int, 8);
pub const __INT_FAST8_FMTd__ = "hhd";
pub const __INT_FAST8_FMTi__ = "hhi";
pub const __UINT_FAST8_TYPE__ = u8;
pub const __UINT_FAST8_MAX__ = @as(c_int, 255);
pub const __UINT_FAST8_FMTo__ = "hho";
pub const __UINT_FAST8_FMTu__ = "hhu";
pub const __UINT_FAST8_FMTx__ = "hhx";
pub const __UINT_FAST8_FMTX__ = "hhX";
pub const __INT_FAST16_TYPE__ = c_short;
pub const __INT_FAST16_MAX__ = @as(c_int, 32767);
pub const __INT_FAST16_WIDTH__ = @as(c_int, 16);
pub const __INT_FAST16_FMTd__ = "hd";
pub const __INT_FAST16_FMTi__ = "hi";
pub const __UINT_FAST16_TYPE__ = c_ushort;
pub const __UINT_FAST16_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_int, 65535, .decimal);
pub const __UINT_FAST16_FMTo__ = "ho";
pub const __UINT_FAST16_FMTu__ = "hu";
pub const __UINT_FAST16_FMTx__ = "hx";
pub const __UINT_FAST16_FMTX__ = "hX";
pub const __INT_FAST32_TYPE__ = c_int;
pub const __INT_FAST32_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __INT_FAST32_WIDTH__ = @as(c_int, 32);
pub const __INT_FAST32_FMTd__ = "d";
pub const __INT_FAST32_FMTi__ = "i";
pub const __UINT_FAST32_TYPE__ = c_uint;
pub const __UINT_FAST32_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_uint, 4294967295, .decimal);
pub const __UINT_FAST32_FMTo__ = "o";
pub const __UINT_FAST32_FMTu__ = "u";
pub const __UINT_FAST32_FMTx__ = "x";
pub const __UINT_FAST32_FMTX__ = "X";
pub const __INT_FAST64_TYPE__ = c_long;
pub const __INT_FAST64_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __INT_FAST64_WIDTH__ = @as(c_int, 64);
pub const __INT_FAST64_FMTd__ = "ld";
pub const __INT_FAST64_FMTi__ = "li";
pub const __UINT_FAST64_TYPE__ = c_ulong;
pub const __UINT_FAST64_MAX__ = @import("std").zig.c_translation.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const __UINT_FAST64_FMTo__ = "lo";
pub const __UINT_FAST64_FMTu__ = "lu";
pub const __UINT_FAST64_FMTx__ = "lx";
pub const __UINT_FAST64_FMTX__ = "lX";
pub const __USER_LABEL_PREFIX__ = "";
pub const __FINITE_MATH_ONLY__ = @as(c_int, 0);
pub const __GNUC_STDC_INLINE__ = @as(c_int, 1);
pub const __GCC_ATOMIC_TEST_AND_SET_TRUEVAL = @as(c_int, 1);
pub const __CLANG_ATOMIC_BOOL_LOCK_FREE = @as(c_int, 2);
pub const __CLANG_ATOMIC_CHAR_LOCK_FREE = @as(c_int, 2);
pub const __CLANG_ATOMIC_CHAR16_T_LOCK_FREE = @as(c_int, 2);
pub const __CLANG_ATOMIC_CHAR32_T_LOCK_FREE = @as(c_int, 2);
pub const __CLANG_ATOMIC_WCHAR_T_LOCK_FREE = @as(c_int, 2);
pub const __CLANG_ATOMIC_SHORT_LOCK_FREE = @as(c_int, 2);
pub const __CLANG_ATOMIC_INT_LOCK_FREE = @as(c_int, 2);
pub const __CLANG_ATOMIC_LONG_LOCK_FREE = @as(c_int, 2);
pub const __CLANG_ATOMIC_LLONG_LOCK_FREE = @as(c_int, 2);
pub const __CLANG_ATOMIC_POINTER_LOCK_FREE = @as(c_int, 2);
pub const __GCC_ATOMIC_BOOL_LOCK_FREE = @as(c_int, 2);
pub const __GCC_ATOMIC_CHAR_LOCK_FREE = @as(c_int, 2);
pub const __GCC_ATOMIC_CHAR16_T_LOCK_FREE = @as(c_int, 2);
pub const __GCC_ATOMIC_CHAR32_T_LOCK_FREE = @as(c_int, 2);
pub const __GCC_ATOMIC_WCHAR_T_LOCK_FREE = @as(c_int, 2);
pub const __GCC_ATOMIC_SHORT_LOCK_FREE = @as(c_int, 2);
pub const __GCC_ATOMIC_INT_LOCK_FREE = @as(c_int, 2);
pub const __GCC_ATOMIC_LONG_LOCK_FREE = @as(c_int, 2);
pub const __GCC_ATOMIC_LLONG_LOCK_FREE = @as(c_int, 2);
pub const __GCC_ATOMIC_POINTER_LOCK_FREE = @as(c_int, 2);
pub const __NO_INLINE__ = @as(c_int, 1);
pub const __PIC__ = @as(c_int, 2);
pub const __pic__ = @as(c_int, 2);
pub const __PIE__ = @as(c_int, 2);
pub const __pie__ = @as(c_int, 2);
pub const __FLT_RADIX__ = @as(c_int, 2);
pub const __DECIMAL_DIG__ = __LDBL_DECIMAL_DIG__;
pub const __GCC_ASM_FLAG_OUTPUTS__ = @as(c_int, 1);
pub const __code_model_small__ = @as(c_int, 1);
pub const __amd64__ = @as(c_int, 1);
pub const __amd64 = @as(c_int, 1);
pub const __x86_64 = @as(c_int, 1);
pub const __x86_64__ = @as(c_int, 1);
pub const __SEG_GS = @as(c_int, 1);
pub const __SEG_FS = @as(c_int, 1);
pub const __corei7 = @as(c_int, 1);
pub const __corei7__ = @as(c_int, 1);
pub const __tune_corei7__ = @as(c_int, 1);
pub const __REGISTER_PREFIX__ = "";
pub const __NO_MATH_INLINES = @as(c_int, 1);
pub const __AES__ = @as(c_int, 1);
pub const __PCLMUL__ = @as(c_int, 1);
pub const __LAHF_SAHF__ = @as(c_int, 1);
pub const __LZCNT__ = @as(c_int, 1);
pub const __RDRND__ = @as(c_int, 1);
pub const __FSGSBASE__ = @as(c_int, 1);
pub const __BMI__ = @as(c_int, 1);
pub const __BMI2__ = @as(c_int, 1);
pub const __POPCNT__ = @as(c_int, 1);
pub const __PRFCHW__ = @as(c_int, 1);
pub const __RDSEED__ = @as(c_int, 1);
pub const __ADX__ = @as(c_int, 1);
pub const __MOVBE__ = @as(c_int, 1);
pub const __FMA__ = @as(c_int, 1);
pub const __F16C__ = @as(c_int, 1);
pub const __FXSR__ = @as(c_int, 1);
pub const __XSAVE__ = @as(c_int, 1);
pub const __XSAVEOPT__ = @as(c_int, 1);
pub const __XSAVEC__ = @as(c_int, 1);
pub const __XSAVES__ = @as(c_int, 1);
pub const __CLFLUSHOPT__ = @as(c_int, 1);
pub const __SGX__ = @as(c_int, 1);
pub const __INVPCID__ = @as(c_int, 1);
pub const __CRC32__ = @as(c_int, 1);
pub const __AVX2__ = @as(c_int, 1);
pub const __AVX__ = @as(c_int, 1);
pub const __SSE4_2__ = @as(c_int, 1);
pub const __SSE4_1__ = @as(c_int, 1);
pub const __SSSE3__ = @as(c_int, 1);
pub const __SSE3__ = @as(c_int, 1);
pub const __SSE2__ = @as(c_int, 1);
pub const __SSE2_MATH__ = @as(c_int, 1);
pub const __SSE__ = @as(c_int, 1);
pub const __SSE_MATH__ = @as(c_int, 1);
pub const __MMX__ = @as(c_int, 1);
pub const __GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 = @as(c_int, 1);
pub const __GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 = @as(c_int, 1);
pub const __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 = @as(c_int, 1);
pub const __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 = @as(c_int, 1);
pub const __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16 = @as(c_int, 1);
pub const __SIZEOF_FLOAT128__ = @as(c_int, 16);
pub const unix = @as(c_int, 1);
pub const __unix = @as(c_int, 1);
pub const __unix__ = @as(c_int, 1);
pub const linux = @as(c_int, 1);
pub const __linux = @as(c_int, 1);
pub const __linux__ = @as(c_int, 1);
pub const __ELF__ = @as(c_int, 1);
pub const __gnu_linux__ = @as(c_int, 1);
pub const __FLOAT128__ = @as(c_int, 1);
pub const __STDC__ = @as(c_int, 1);
pub const __STDC_HOSTED__ = @as(c_int, 1);
pub const __STDC_VERSION__ = @as(c_long, 201710);
pub const __STDC_UTF_16__ = @as(c_int, 1);
pub const __STDC_UTF_32__ = @as(c_int, 1);
pub const _DEBUG = @as(c_int, 1);
pub const __GCC_HAVE_DWARF2_CFI_ASM = @as(c_int, 1);
pub const __ANIVA_ZIG_ABI_BINDINGS__ = "";
pub const __ANIVA_KDEV_CORE__ = "";
pub const __ANIVA_HIVE__ = "";
pub const __ANIVA_LINKEDLIST__ = "";
pub const __ANIVA_STDDEF__ = "";
pub const __user = "";
pub inline fn FuncPtrWith(@"type": anytype, name: anytype) @TypeOf(@"type"(name.*)()) {
    return @"type"(name.*)();
}
pub const nullptr = @import("std").zig.c_translation.cast(?*anyopaque, @as(c_int, 0));
pub const NULL = @as(c_int, 0);
pub const @"true" = @as(c_int, 1);
pub const @"false" = @as(c_int, 0);
pub const STATIC_CAST = @import("std").zig.c_translation.Macros.CAST_OR_CALL;
pub const Kib = @as(c_int, 1024);
pub const Mib = Kib * Kib;
pub const Gib = Mib * Kib;
pub const Tib = Gib * Kib;
pub const __ANIVA_ERROR_WRAPPER__ = "";
pub const __ANIVA_LOGGING__ = "";
pub const LOGGER_MAX_COUNT = @as(c_int, 255);
pub const LOGGER_FLAG_DEBUG = @as(c_int, 0x01);
pub const LOGGER_FLAG_INFO = @as(c_int, 0x02);
pub const LOGGER_FLAG_WARNINGS = @as(c_int, 0x04);
pub const LOG_TYPE_DEFAULT = @as(c_int, 0);
pub const LOG_TYPE_FMT = @as(c_int, 1);
pub const LOG_TYPE_LINE = @as(c_int, 2);
pub const LOG_TYPE_CHAR = @as(c_int, 3);
pub const LOG_TYPE_ERROR = @as(c_int, 4);
pub const LOG_TYPE_WARNING = @as(c_int, 5);
pub const KERR_FATAL_BIT = @as(c_ulong, 0x80000000);
pub const KERR_NONE = @as(c_int, 0);
pub const KERR_INVAL = @as(c_int, 1);
pub const KERR_IO = @as(c_int, 2);
pub const KERR_NOMEM = @as(c_int, 3);
pub const KERR_MEM = @as(c_int, 4);
pub const KERR_NODEV = @as(c_int, 5);
pub const KERR_DEV = @as(c_int, 6);
pub const KERR_NODRV = @as(c_int, 7);
pub const KERR_DRV = @as(c_int, 8);
pub const KERR_NOCON = @as(c_int, 9);
pub const KERR_NOT_FOUND = @as(c_int, 10);
pub const KERR_SIZE_MISMATCH = @as(c_int, 11);
pub const KERR_NULL = @as(c_int, 12);
pub const EOP_ERROR = @as(c_int, 0);
pub const EOP_NOMEM = @as(c_int, 1);
pub const EOP_INVAL = @as(c_int, 2);
pub const EOP_BUSY = @as(c_int, 3);
pub const EOP_NODEV = @as(c_int, 4);
pub const EOP_NOCON = @as(c_int, 5);
pub const EOP_NULL = @as(c_int, 6);
pub const HIVE_PART_SEPERATOR = '/';
pub const __ANIVA_PACKET_RESPONSE__ = "";
pub const DRIVER_URL_SEPERATOR = '/';
pub const DRIVER_TYPE_COUNT = @as(c_int, 9);
pub const DRIVER_WAIT_UNTIL_READY = @import("std").zig.c_translation.cast(usize, -@as(c_int, 1));
pub const DT_DISK = @as(c_int, 0);
pub const DT_FS = @as(c_int, 1);
pub const DT_IO = @as(c_int, 2);
pub const DT_SOUND = @as(c_int, 3);
pub const DT_GRAPHICS = @as(c_int, 4);
pub const DT_SERVICE = @as(c_int, 5);
pub const DT_DIAGNOSTICS = @as(c_int, 6);
pub const DT_OTHER = @as(c_int, 7);
pub const DT_FIRMWARE = @as(c_int, 8);
pub inline fn VALID_DEV_TYPE(@"type": anytype) @TypeOf((@"type" != 0) and (@"type" < DRIVER_TYPE_COUNT)) {
    return (@"type" != 0) and (@"type" < DRIVER_TYPE_COUNT);
}
pub const DRV_INFINITE = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xFFFFFFFF, .hexadecimal);
pub const DRV_SERVICE_MAX = @as(c_int, 0x100);
pub const drv_manifest_SOFTMAX = @as(c_int, 512);
pub const DCC_DATA = @import("std").zig.c_translation.cast(u32, @as(c_int, 0));
pub const DCC_EXIT = @import("std").zig.c_translation.cast(u32, -@as(c_int, 1));
pub const DCC_RESPONSE = @import("std").zig.c_translation.cast(u32, -@as(c_int, 2));
pub const DCC_WRITE = @import("std").zig.c_translation.cast(u32, -@as(c_int, 3));
pub const MAX_DRIVER_NAME_LENGTH = @as(c_int, 128);
pub const MAX_DRIVER_DESCRIPTOR_LENGTH = @as(c_int, 64);
pub inline fn SOCKET_VERIFY_RESPONSE_SIZE(size: anytype) @TypeOf(size != @import("std").zig.c_translation.cast(usize, -@as(c_int, 1))) {
    return size != @import("std").zig.c_translation.cast(usize, -@as(c_int, 1));
}
pub const NO_MANIFEST = NULL;
pub const __ANIVA_DEV_DEVICE__ = "";
pub const __ANIVA_DEVICE_ENDPOINT__ = "";
pub const __ANIVA_DEV_GROUP__ = "";
pub const DGROUP_FLAG_HIDDEN = @as(c_int, 0x00000001);
pub const DGROUP_FLAG_BUS = @as(c_int, 0x00000002);
pub const __ANIVA_SYNC_MUTEX__ = "";
pub const __ANIVA_LIBK_QUEUE__ = "";
pub const __ANIVA_THREAD__ = "";
pub const __entry__ = "";
pub const PDE_PRESENT = @as(c_ulonglong, 0x0000000000000001);
pub const PDE_WRITABLE = @as(c_ulonglong, 0x0000000000000002);
pub const PDE_USER = @as(c_ulonglong, 0x0000000000000004);
pub const PDE_WRITE_THROUGH = @as(c_ulonglong, 0x0000000000000008);
pub const PDE_NO_CACHE = @as(c_ulonglong, 0x0000000000000010);
pub const PDE_ACCESSED = @as(c_ulonglong, 0x0000000000000020);
pub const PDE_DIRTY = @as(c_ulonglong, 0x0000000000000040);
pub const PDE_HUGE_PAGE = @as(c_ulonglong, 0x0000000000000080);
pub const PDE_GLOBAL = @as(c_ulonglong, 0x0000000000000100);
pub const PDE_PAT = @as(c_ulonglong, 0x0000000000000800);
pub const PDE_NX = @as(c_ulonglong, 0x8000000000000000);
pub const PTE_PRESENT = @as(c_ulonglong, 0x0000000000000001);
pub const PTE_WRITABLE = @as(c_ulonglong, 0x0000000000000002);
pub const PTE_USER = @as(c_ulonglong, 0x0000000000000004);
pub const PTE_WRITE_THROUGH = @as(c_ulonglong, 0x0000000000000008);
pub const PTE_NO_CACHE = @as(c_ulonglong, 0x0000000000000010);
pub const PTE_ACCESSED = @as(c_ulonglong, 0x0000000000000020);
pub const PTE_DIRTY = @as(c_ulonglong, 0x0000000000000040);
pub const PTE_PAT = @as(c_ulonglong, 0x0000000000000080);
pub const PTE_GLOBAL = @as(c_ulonglong, 0x0000000000000100);
pub const PTE_NX = @as(c_ulonglong, 0x8000000000000000);
pub const __ANIVA_KERNEL_CONTEXT__ = "";
pub const __GDT__ = "";
pub const GDT_NULL_SELC = @as(c_int, 0);
pub const GDT_KERNEL_CODE = @as(c_int, 0x08);
pub const GDT_KERNEL_DATA = @as(c_int, 0x10);
pub const GDT_USER_DATA = @as(c_int, 0x18);
pub const GDT_USER_CODE = @as(c_int, 0x20);
pub const GDT_TSS_SEL = @as(c_int, 0x28);
pub const GDT_TSS_2_SEL = @as(c_int, 0x30);
pub const AVAILABLE_TSS = @as(c_int, 0x9);
pub const BUSY_TSS = @as(c_int, 0xb);
pub const EFLAGS_CF = @as(c_int, 0);
pub const EFLAGS_RES = @as(c_int, 1) << @as(c_int, 1);
pub const EFLAGS_PF = @as(c_int, 1) << @as(c_int, 2);
pub const EFLAGS_AF = @as(c_int, 1) << @as(c_int, 4);
pub const EFLAGS_ZF = @as(c_int, 1) << @as(c_int, 6);
pub const EFLAGS_SF = @as(c_int, 1) << @as(c_int, 7);
pub const EFLAGS_TF = @as(c_int, 1) << @as(c_int, 8);
pub const EFLAGS_IF = @as(c_int, 1) << @as(c_int, 9);
pub const EFLAGS_DF = @as(c_int, 1) << @as(c_int, 10);
pub const EFLAGS_OF = @as(c_int, 1) << @as(c_int, 11);
pub const EFLAGS_IOPL = (@as(c_int, 1) << @as(c_int, 12)) | (@as(c_int, 1) << @as(c_int, 13));
pub const EFLAGS_NT = @as(c_int, 1) << @as(c_int, 14);
pub const EFLAGS_RF = @as(c_int, 1) << @as(c_int, 16);
pub const EFLAGS_VM = @as(c_int, 1) << @as(c_int, 17);
pub const EFLAGS_AC = @as(c_int, 1) << @as(c_int, 18);
pub const EFLAGS_VIF = @as(c_int, 1) << @as(c_int, 19);
pub const EFLAGS_VIP = @as(c_int, 1) << @as(c_int, 20);
pub const EFLAGS_ID = @as(c_int, 1) << @as(c_int, 21);
pub const THRD_CTX_STACK_BLOCK_SIZE = @as(c_int, 128);
pub const __ANIVA_SOCKET_THREAD__ = "";
pub const __ANIVA_PACKET_PAYLOAD__ = "";
pub const __ANIVA_SPINLOCK__ = "";
pub const __ANIVA_ATOMIC_PTR__ = "";
pub const __ANIVA_ATOMIC__ = "";
pub const __ANIVA_REGISTERS__ = "";
pub const __ANIVA_FPU_STATE__ = "";
pub const __ANIVA_PROC_CORE__ = "";
pub const __ANIVA_VECTOR__ = "";
pub const VEC_FLAG_FLEXIBLE = @as(c_int, 0x0001);
pub const VEC_FLAG_NO_DUPLICATES = @as(c_int, 0x0002);
pub const DEFAULT_STACK_SIZE = @as(c_int, 32) * Kib;
pub const DEFAULT_THREAD_MAX_TICKS = @as(c_int, 10);
pub const PROC_DEFAULT_MAX_THREADS = @as(c_int, 16);
pub const PROC_CORE_PROCESS_NAME = "[aniva-core]";
pub const PROC_MAX_TICKS = @as(c_int, 20);
pub const PROC_SOFTMAX = @as(c_int, 0x1000);
pub const PROC_HARDMAX = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xFFFFFFFF, .hexadecimal);
pub const THREAD_HARDMAX = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xFFFFFFFF, .hexadecimal);
pub const MIN_SOCKET_BUFFER_SIZE = @as(c_int, 0);
pub const SOCKET_DEFAULT_SOCKET_BUFFER_SIZE = @as(c_int, 4096);
pub const SOCKET_DEFAULT_MAXIMUM_SOCKET_COUNT = @as(c_int, 128);
pub const SOCKET_DEFAULT_MAXIMUM_BUFFER_COUNT = @as(c_int, 64);
pub inline fn T_SOCKET(t: anytype) @TypeOf(t.*.m_socket) {
    return t.*.m_socket;
}
pub const THREAD_PUSH_REGS = "pushfq \n" ++ "pushq %%r15 \n" ++ "pushq %%r14 \n" ++ "pushq %%r13 \n" ++ "pushq %%r12 \n" ++ "pushq %%r11 \n" ++ "pushq %%r10 \n" ++ "pushq %%r9 \n" ++ "pushq %%r8 \n" ++ "pushq %%rax \n" ++ "pushq %%rbx \n" ++ "pushq %%rcx \n" ++ "pushq %%rdx \n" ++ "pushq %%rbp \n" ++ "pushq %%rsi \n" ++ "pushq %%rdi \n";
pub const THREAD_POP_REGS = "popq %%rdi \n" ++ "popq %%rsi \n" ++ "popq %%rbp \n" ++ "popq %%rdx \n" ++ "popq %%rcx \n" ++ "popq %%rbx \n" ++ "popq %%rax \n" ++ "popq %%r8 \n" ++ "popq %%r9 \n" ++ "popq %%r10 \n" ++ "popq %%r11 \n" ++ "popq %%r12 \n" ++ "popq %%r13 \n" ++ "popq %%r14 \n" ++ "popq %%r15 \n" ++ "popfq \n";
pub const MUTEX_FLAG_IS_HELD = @as(c_int, 0x01);
pub const MUTEX_FLAG_IS_SHARED = @as(c_int, 0x02);
pub const __ANIVA_ACPI_DEVICE__ = "";
pub const __ACTYPES_H__ = "";
pub const __ACENV_H__ = "";
pub const ACPI_BINARY_SEMAPHORE = @as(c_int, 0);
pub const ACPI_OSL_MUTEX = @as(c_int, 1);
pub const DEBUGGER_SINGLE_THREADED = @as(c_int, 0);
pub const DEBUGGER_MULTI_THREADED = @as(c_int, 1);
pub const ACPI_SRC_OS_LF_ONLY = @as(c_int, 0);
pub const __ACGCC_H__ = "";
pub const COMPILER_VA_MACRO = @as(c_int, 1);
pub const ACPI_USE_NATIVE_MATH64 = "";
pub const __ANIVA_ACPICA_AC__ = "";
pub const __ANIVA_ZALLOC__ = "";
pub const __ANIVA_BITMAP__ = "";
pub inline fn BITS_TO_BYTES(bits: anytype) @TypeOf(bits >> @as(c_int, 3)) {
    return bits >> @as(c_int, 3);
}
pub inline fn BYTES_TO_BITS(bytes: anytype) @TypeOf(bytes << @as(c_int, 3)) {
    return bytes << @as(c_int, 3);
}
pub const DEFAULT_ZONE_ENTRY_SIZE_COUNT = @as(c_int, 8);
pub const ZALLOC_FLAG_KERNEL = @as(c_int, 0x00000001);
pub const ZALLOC_DEFAULT_MEM_SIZE = @as(c_int, 16) * Kib;
pub const ZALLOC_DEFAULT_ALLOC_COUNT = @as(c_int, 128);
pub const ZALLOC_ACCEPTABLE_MEMSIZE_DEVIATON = @as(c_int, 16);
pub const __ANIVA_LIBK_CTYPE__ = "";
pub const _U = @as(c_int, 0o1);
pub const _L = @as(c_int, 0o2);
pub const _N = @as(c_int, 0o4);
pub const _S = @as(c_int, 0o10);
pub const _P = @as(c_int, 0o20);
pub const _C = @as(c_int, 0o40);
pub const _X = @as(c_int, 0o100);
pub const _B = @as(c_int, 0o200);
pub const __ANIVA_STRING__ = "";
pub const __ANIVA_ASM_SPECIFICS__ = "";
pub const ACPI_USE_SYSTEM_CLIBRARY = "";
pub const ACPI_USE_DO_WHILE_0 = "";
pub const ACPI_IGNORE_PACKAGE_RESOLUTION_ERRORS = "";
pub const ACPI_USE_GPE_POLLING = "";
pub const ACPI_MUTEX_TYPE = ACPI_OSL_MUTEX;
pub const ACPI_DEBUGGER = "";
pub const ACPI_DEBUG_OUTPUT = "";
pub const ACPI_SUPRESS_WARNINGS = "";
pub const ACPI_INIT_FUNCTION = __init;
pub const ACPI_MACHINE_WIDTH = @as(c_int, 64);
pub const strtoul = dirty_strtoul;
pub const ACPI_CACHE_T = struct_zone_allocator;
pub const ACPI_CPU_FLAGS = c_uint;
pub const ACPI_UINTPTR_T = usize;
pub inline fn ACPI_TO_INTEGER(p: anytype) usize {
    return @import("std").zig.c_translation.cast(usize, p);
}
pub inline fn ACPI_OFFSET(d: anytype, f: anytype) @TypeOf(GET_OFFSET(d, f)) {
    return GET_OFFSET(d, f);
}
pub const ACPI_MSG_ERROR = "ACPI Error: ";
pub const ACPI_MSG_EXCEPTION = "ACPI Exception: ";
pub const ACPI_MSG_WARNING = "ACPI Warning: ";
pub const ACPI_MSG_INFO = "ACPI: ";
pub const ACPI_MSG_BIOS_ERROR = "ACPI BIOS Error (bug): ";
pub const ACPI_MSG_BIOS_WARNING = "ACPI BIOS Warning (bug): ";
pub const COMPILER_DEPENDENT_INT64 = c_longlong;
pub const COMPILER_DEPENDENT_UINT64 = c_ulonglong;
pub const ACPI_SYSTEM_XFACE = "";
pub const ACPI_EXTERNAL_XFACE = "";
pub const ACPI_INTERNAL_XFACE = "";
pub const ACPI_INTERNAL_VAR_XFACE = "";
pub const DEBUGGER_THREADING = DEBUGGER_MULTI_THREADED;
pub const ACPI_FILE = ?*anyopaque;
pub const ACPI_FILE_OUT = NULL;
pub const ACPI_FILE_ERR = NULL;
pub const ACPI_UINT8_MAX = @import("std").zig.c_translation.cast(UINT8, ~@import("std").zig.c_translation.cast(UINT8, @as(c_int, 0)));
pub const ACPI_UINT16_MAX = @import("std").zig.c_translation.cast(UINT16, ~@import("std").zig.c_translation.cast(UINT16, @as(c_int, 0)));
pub const ACPI_UINT32_MAX = @import("std").zig.c_translation.cast(UINT32, ~@import("std").zig.c_translation.cast(UINT32, @as(c_int, 0)));
pub const ACPI_UINT64_MAX = @import("std").zig.c_translation.cast(UINT64, ~@import("std").zig.c_translation.cast(UINT64, @as(c_int, 0)));
pub const ACPI_ASCII_MAX = @as(c_int, 0x7F);
pub const ACPI_THREAD_ID = UINT64;
pub const ACPI_MAX_PTR = ACPI_UINT64_MAX;
pub const ACPI_SIZE_MAX = ACPI_UINT64_MAX;
pub const ACPI_USE_NATIVE_DIVIDE = "";
pub const ACPI_SEMAPHORE = ?*anyopaque;
pub const ACPI_MUTEX = ?*anyopaque;
pub const ACPI_MAX_GPE_BLOCKS = @as(c_int, 2);
pub const ACPI_GPE_REGISTER_WIDTH = @as(c_int, 8);
pub const ACPI_PM1_REGISTER_WIDTH = @as(c_int, 16);
pub const ACPI_PM2_REGISTER_WIDTH = @as(c_int, 8);
pub const ACPI_PM_TIMER_WIDTH = @as(c_int, 32);
pub const ACPI_RESET_REGISTER_WIDTH = @as(c_int, 8);
pub const ACPI_NAMESEG_SIZE = @as(c_int, 4);
pub const ACPI_PATH_SEGMENT_LENGTH = @as(c_int, 5);
pub const ACPI_PATH_SEPARATOR = '.';
pub const ACPI_OEM_ID_SIZE = @as(c_int, 6);
pub const ACPI_OEM_TABLE_ID_SIZE = @as(c_int, 8);
pub const PCI_ROOT_HID_STRING = "PNP0A03";
pub const PCI_EXPRESS_ROOT_HID_STRING = "PNP0A08";
pub const ACPI_PM_TIMER_FREQUENCY = @import("std").zig.c_translation.promoteIntLiteral(c_int, 3579545, .decimal);
pub const FALSE = @as(c_int, 1) == @as(c_int, 0);
pub const TRUE = @as(c_int, 1) == @as(c_int, 1);
pub const ACPI_MSEC_PER_SEC = @as(c_long, 1000);
pub const ACPI_USEC_PER_MSEC = @as(c_long, 1000);
pub const ACPI_USEC_PER_SEC = @as(c_long, 1000000);
pub const ACPI_100NSEC_PER_USEC = @as(c_long, 10);
pub const ACPI_100NSEC_PER_MSEC = @as(c_long, 10000);
pub const ACPI_100NSEC_PER_SEC = @as(c_long, 10000000);
pub const ACPI_NSEC_PER_USEC = @as(c_long, 1000);
pub const ACPI_NSEC_PER_MSEC = @as(c_long, 1000000);
pub const ACPI_NSEC_PER_SEC = @as(c_long, 1000000000);
pub inline fn ACPI_TIME_AFTER(a: anytype, b: anytype) @TypeOf(@import("std").zig.c_translation.cast(INT64, b - a) < @as(c_int, 0)) {
    return @import("std").zig.c_translation.cast(INT64, b - a) < @as(c_int, 0);
}
pub const ACPI_OWNER_ID_MAX = @as(c_int, 0xFFF);
pub const ACPI_INTEGER_BIT_SIZE = @as(c_int, 64);
pub const ACPI_MAX_DECIMAL_DIGITS = @as(c_int, 20);
pub const ACPI_MAX64_DECIMAL_DIGITS = @as(c_int, 20);
pub const ACPI_MAX32_DECIMAL_DIGITS = @as(c_int, 10);
pub const ACPI_MAX16_DECIMAL_DIGITS = @as(c_int, 5);
pub const ACPI_MAX8_DECIMAL_DIGITS = @as(c_int, 3);
pub const ACPI_ROOT_OBJECT = @import("std").zig.c_translation.cast(ACPI_HANDLE, ACPI_TO_POINTER(ACPI_MAX_PTR));
pub const ACPI_WAIT_FOREVER = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xFFFF, .hexadecimal);
pub const ACPI_DO_NOT_WAIT = @as(c_int, 0);
pub const ACPI_INTEGER_MAX = ACPI_UINT64_MAX;
pub inline fn ACPI_LOBYTE(Integer: anytype) UINT8 {
    return @import("std").zig.c_translation.cast(UINT8, @import("std").zig.c_translation.cast(UINT16, Integer));
}
pub inline fn ACPI_HIBYTE(Integer: anytype) UINT8 {
    return @import("std").zig.c_translation.cast(UINT8, @import("std").zig.c_translation.cast(UINT16, Integer) >> @as(c_int, 8));
}
pub inline fn ACPI_LOWORD(Integer: anytype) UINT16 {
    return @import("std").zig.c_translation.cast(UINT16, @import("std").zig.c_translation.cast(UINT32, Integer));
}
pub inline fn ACPI_HIWORD(Integer: anytype) UINT16 {
    return @import("std").zig.c_translation.cast(UINT16, @import("std").zig.c_translation.cast(UINT32, Integer) >> @as(c_int, 16));
}
pub inline fn ACPI_LODWORD(Integer64: anytype) UINT32 {
    return @import("std").zig.c_translation.cast(UINT32, @import("std").zig.c_translation.cast(UINT64, Integer64));
}
pub inline fn ACPI_HIDWORD(Integer64: anytype) UINT32 {
    return @import("std").zig.c_translation.cast(UINT32, @import("std").zig.c_translation.cast(UINT64, Integer64) >> @as(c_int, 32));
}
pub inline fn ACPI_MIN(a: anytype, b: anytype) @TypeOf(if (a < b) a else b) {
    return if (a < b) a else b;
}
pub inline fn ACPI_MAX(a: anytype, b: anytype) @TypeOf(if (a > b) a else b) {
    return if (a > b) a else b;
}
pub inline fn ACPI_ADD_PTR(t: anytype, a: anytype, b: anytype) @TypeOf(ACPI_CAST_PTR(t, ACPI_CAST_PTR(UINT8, a) + @import("std").zig.c_translation.cast(ACPI_SIZE, b))) {
    return ACPI_CAST_PTR(t, ACPI_CAST_PTR(UINT8, a) + @import("std").zig.c_translation.cast(ACPI_SIZE, b));
}
pub inline fn ACPI_SUB_PTR(t: anytype, a: anytype, b: anytype) @TypeOf(ACPI_CAST_PTR(t, ACPI_CAST_PTR(UINT8, a) - @import("std").zig.c_translation.cast(ACPI_SIZE, b))) {
    return ACPI_CAST_PTR(t, ACPI_CAST_PTR(UINT8, a) - @import("std").zig.c_translation.cast(ACPI_SIZE, b));
}
pub inline fn ACPI_PTR_DIFF(a: anytype, b: anytype) ACPI_SIZE {
    return @import("std").zig.c_translation.cast(ACPI_SIZE, ACPI_CAST_PTR(UINT8, a) - ACPI_CAST_PTR(UINT8, b));
}
pub inline fn ACPI_TO_POINTER(i: anytype) @TypeOf(ACPI_CAST_PTR(anyopaque, @import("std").zig.c_translation.cast(ACPI_SIZE, i))) {
    return ACPI_CAST_PTR(anyopaque, @import("std").zig.c_translation.cast(ACPI_SIZE, i));
}
pub inline fn ACPI_PTR_TO_PHYSADDR(i: anytype) @TypeOf(ACPI_TO_INTEGER(i)) {
    return ACPI_TO_INTEGER(i);
}
pub inline fn ACPI_COMPARE_NAMESEG(a: anytype, b: anytype) @TypeOf(ACPI_CAST_PTR(UINT32, a).* == ACPI_CAST_PTR(UINT32, b).*) {
    return ACPI_CAST_PTR(UINT32, a).* == ACPI_CAST_PTR(UINT32, b).*;
}
pub inline fn ACPI_VALIDATE_RSDP_SIG(a: anytype) @TypeOf(!(strncmp(ACPI_CAST_PTR(u8, a), ACPI_SIG_RSDP, @as(c_int, 8)) != 0)) {
    return !(strncmp(ACPI_CAST_PTR(u8, a), ACPI_SIG_RSDP, @as(c_int, 8)) != 0);
}
pub inline fn ACPI_MAKE_RSDP_SIG(dest: anytype) @TypeOf(memcpy(ACPI_CAST_PTR(u8, dest), ACPI_SIG_RSDP, @as(c_int, 8))) {
    return memcpy(ACPI_CAST_PTR(u8, dest), ACPI_SIG_RSDP, @as(c_int, 8));
}
pub const ACPI_ACCESS_BIT_SHIFT = @as(c_int, 2);
pub const ACPI_ACCESS_BYTE_SHIFT = -@as(c_int, 1);
pub const ACPI_ACCESS_BIT_MAX = @as(c_int, 31) - ACPI_ACCESS_BIT_SHIFT;
pub const ACPI_ACCESS_BYTE_MAX = @as(c_int, 31) - ACPI_ACCESS_BYTE_SHIFT;
pub const ACPI_ACCESS_BIT_DEFAULT = @as(c_int, 8) - ACPI_ACCESS_BIT_SHIFT;
pub const ACPI_ACCESS_BYTE_DEFAULT = @as(c_int, 8) - ACPI_ACCESS_BYTE_SHIFT;
pub inline fn ACPI_ACCESS_BIT_WIDTH(size: anytype) @TypeOf(@as(c_int, 1) << (size + ACPI_ACCESS_BIT_SHIFT)) {
    return @as(c_int, 1) << (size + ACPI_ACCESS_BIT_SHIFT);
}
pub inline fn ACPI_ACCESS_BYTE_WIDTH(size: anytype) @TypeOf(@as(c_int, 1) << (size + ACPI_ACCESS_BYTE_SHIFT)) {
    return @as(c_int, 1) << (size + ACPI_ACCESS_BYTE_SHIFT);
}
pub const ACPI_FULL_INITIALIZATION = @as(c_int, 0x0000);
pub const ACPI_NO_FACS_INIT = @as(c_int, 0x0001);
pub const ACPI_NO_ACPI_ENABLE = @as(c_int, 0x0002);
pub const ACPI_NO_HARDWARE_INIT = @as(c_int, 0x0004);
pub const ACPI_NO_EVENT_INIT = @as(c_int, 0x0008);
pub const ACPI_NO_HANDLER_INIT = @as(c_int, 0x0010);
pub const ACPI_NO_OBJECT_INIT = @as(c_int, 0x0020);
pub const ACPI_NO_DEVICE_INIT = @as(c_int, 0x0040);
pub const ACPI_NO_ADDRESS_SPACE_INIT = @as(c_int, 0x0080);
pub const ACPI_SUBSYSTEM_INITIALIZE = @as(c_int, 0x01);
pub const ACPI_INITIALIZED_OK = @as(c_int, 0x02);
pub const ACPI_STATE_UNKNOWN = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0xFF));
pub const ACPI_STATE_S0 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0));
pub const ACPI_STATE_S1 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 1));
pub const ACPI_STATE_S2 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 2));
pub const ACPI_STATE_S3 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 3));
pub const ACPI_STATE_S4 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 4));
pub const ACPI_STATE_S5 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 5));
pub const ACPI_S_STATES_MAX = ACPI_STATE_S5;
pub const ACPI_S_STATE_COUNT = @as(c_int, 6);
pub const ACPI_STATE_D0 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0));
pub const ACPI_STATE_D1 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 1));
pub const ACPI_STATE_D2 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 2));
pub const ACPI_STATE_D3 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 3));
pub const ACPI_D_STATES_MAX = ACPI_STATE_D3;
pub const ACPI_D_STATE_COUNT = @as(c_int, 4);
pub const ACPI_STATE_C0 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0));
pub const ACPI_STATE_C1 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 1));
pub const ACPI_STATE_C2 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 2));
pub const ACPI_STATE_C3 = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 3));
pub const ACPI_C_STATES_MAX = ACPI_STATE_C3;
pub const ACPI_C_STATE_COUNT = @as(c_int, 4);
pub const ACPI_SLEEP_TYPE_MAX = @as(c_int, 0x7);
pub const ACPI_SLEEP_TYPE_INVALID = @as(c_int, 0xFF);
pub const ACPI_NOTIFY_BUS_CHECK = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x00));
pub const ACPI_NOTIFY_DEVICE_CHECK = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x01));
pub const ACPI_NOTIFY_DEVICE_WAKE = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x02));
pub const ACPI_NOTIFY_EJECT_REQUEST = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x03));
pub const ACPI_NOTIFY_DEVICE_CHECK_LIGHT = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x04));
pub const ACPI_NOTIFY_FREQUENCY_MISMATCH = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x05));
pub const ACPI_NOTIFY_BUS_MODE_MISMATCH = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x06));
pub const ACPI_NOTIFY_POWER_FAULT = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x07));
pub const ACPI_NOTIFY_CAPABILITIES_CHECK = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x08));
pub const ACPI_NOTIFY_DEVICE_PLD_CHECK = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x09));
pub const ACPI_NOTIFY_RESERVED = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x0A));
pub const ACPI_NOTIFY_LOCALITY_UPDATE = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x0B));
pub const ACPI_NOTIFY_SHUTDOWN_REQUEST = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x0C));
pub const ACPI_NOTIFY_AFFINITY_UPDATE = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x0D));
pub const ACPI_NOTIFY_MEMORY_UPDATE = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x0E));
pub const ACPI_NOTIFY_DISCONNECT_RECOVER = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x0F));
pub const ACPI_GENERIC_NOTIFY_MAX = @as(c_int, 0x0F);
pub const ACPI_SPECIFIC_NOTIFY_MAX = @as(c_int, 0x84);
pub const ACPI_TYPE_ANY = @as(c_int, 0x00);
pub const ACPI_TYPE_INTEGER = @as(c_int, 0x01);
pub const ACPI_TYPE_STRING = @as(c_int, 0x02);
pub const ACPI_TYPE_BUFFER = @as(c_int, 0x03);
pub const ACPI_TYPE_PACKAGE = @as(c_int, 0x04);
pub const ACPI_TYPE_FIELD_UNIT = @as(c_int, 0x05);
pub const ACPI_TYPE_DEVICE = @as(c_int, 0x06);
pub const ACPI_TYPE_EVENT = @as(c_int, 0x07);
pub const ACPI_TYPE_METHOD = @as(c_int, 0x08);
pub const ACPI_TYPE_MUTEX = @as(c_int, 0x09);
pub const ACPI_TYPE_REGION = @as(c_int, 0x0A);
pub const ACPI_TYPE_POWER = @as(c_int, 0x0B);
pub const ACPI_TYPE_PROCESSOR = @as(c_int, 0x0C);
pub const ACPI_TYPE_THERMAL = @as(c_int, 0x0D);
pub const ACPI_TYPE_BUFFER_FIELD = @as(c_int, 0x0E);
pub const ACPI_TYPE_DDB_HANDLE = @as(c_int, 0x0F);
pub const ACPI_TYPE_DEBUG_OBJECT = @as(c_int, 0x10);
pub const ACPI_TYPE_EXTERNAL_MAX = @as(c_int, 0x10);
pub const ACPI_NUM_TYPES = ACPI_TYPE_EXTERNAL_MAX + @as(c_int, 1);
pub const ACPI_TYPE_LOCAL_REGION_FIELD = @as(c_int, 0x11);
pub const ACPI_TYPE_LOCAL_BANK_FIELD = @as(c_int, 0x12);
pub const ACPI_TYPE_LOCAL_INDEX_FIELD = @as(c_int, 0x13);
pub const ACPI_TYPE_LOCAL_REFERENCE = @as(c_int, 0x14);
pub const ACPI_TYPE_LOCAL_ALIAS = @as(c_int, 0x15);
pub const ACPI_TYPE_LOCAL_METHOD_ALIAS = @as(c_int, 0x16);
pub const ACPI_TYPE_LOCAL_NOTIFY = @as(c_int, 0x17);
pub const ACPI_TYPE_LOCAL_ADDRESS_HANDLER = @as(c_int, 0x18);
pub const ACPI_TYPE_LOCAL_RESOURCE = @as(c_int, 0x19);
pub const ACPI_TYPE_LOCAL_RESOURCE_FIELD = @as(c_int, 0x1A);
pub const ACPI_TYPE_LOCAL_SCOPE = @as(c_int, 0x1B);
pub const ACPI_TYPE_NS_NODE_MAX = @as(c_int, 0x1B);
pub const ACPI_TOTAL_TYPES = ACPI_TYPE_NS_NODE_MAX + @as(c_int, 1);
pub const ACPI_TYPE_LOCAL_EXTRA = @as(c_int, 0x1C);
pub const ACPI_TYPE_LOCAL_DATA = @as(c_int, 0x1D);
pub const ACPI_TYPE_LOCAL_MAX = @as(c_int, 0x1D);
pub const ACPI_TYPE_INVALID = @as(c_int, 0x1E);
pub const ACPI_TYPE_NOT_FOUND = @as(c_int, 0xFF);
pub const ACPI_NUM_NS_TYPES = ACPI_TYPE_INVALID + @as(c_int, 1);
pub const ACPI_READ = @as(c_int, 0);
pub const ACPI_WRITE = @as(c_int, 1);
pub const ACPI_IO_MASK = @as(c_int, 1);
pub const ACPI_EVENT_PMTIMER = @as(c_int, 0);
pub const ACPI_EVENT_GLOBAL = @as(c_int, 1);
pub const ACPI_EVENT_POWER_BUTTON = @as(c_int, 2);
pub const ACPI_EVENT_SLEEP_BUTTON = @as(c_int, 3);
pub const ACPI_EVENT_RTC = @as(c_int, 4);
pub const ACPI_EVENT_MAX = @as(c_int, 4);
pub const ACPI_NUM_FIXED_EVENTS = ACPI_EVENT_MAX + @as(c_int, 1);
pub const ACPI_EVENT_FLAG_DISABLED = @import("std").zig.c_translation.cast(ACPI_EVENT_STATUS, @as(c_int, 0x00));
pub const ACPI_EVENT_FLAG_ENABLED = @import("std").zig.c_translation.cast(ACPI_EVENT_STATUS, @as(c_int, 0x01));
pub const ACPI_EVENT_FLAG_WAKE_ENABLED = @import("std").zig.c_translation.cast(ACPI_EVENT_STATUS, @as(c_int, 0x02));
pub const ACPI_EVENT_FLAG_STATUS_SET = @import("std").zig.c_translation.cast(ACPI_EVENT_STATUS, @as(c_int, 0x04));
pub const ACPI_EVENT_FLAG_ENABLE_SET = @import("std").zig.c_translation.cast(ACPI_EVENT_STATUS, @as(c_int, 0x08));
pub const ACPI_EVENT_FLAG_HAS_HANDLER = @import("std").zig.c_translation.cast(ACPI_EVENT_STATUS, @as(c_int, 0x10));
pub const ACPI_EVENT_FLAG_MASKED = @import("std").zig.c_translation.cast(ACPI_EVENT_STATUS, @as(c_int, 0x20));
pub const ACPI_EVENT_FLAG_SET = ACPI_EVENT_FLAG_STATUS_SET;
pub const ACPI_GPE_ENABLE = @as(c_int, 0);
pub const ACPI_GPE_DISABLE = @as(c_int, 1);
pub const ACPI_GPE_CONDITIONAL_ENABLE = @as(c_int, 2);
pub const ACPI_GPE_DISPATCH_NONE = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x00));
pub const ACPI_GPE_DISPATCH_METHOD = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x01));
pub const ACPI_GPE_DISPATCH_HANDLER = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x02));
pub const ACPI_GPE_DISPATCH_NOTIFY = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x03));
pub const ACPI_GPE_DISPATCH_RAW_HANDLER = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x04));
pub const ACPI_GPE_DISPATCH_MASK = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x07));
pub inline fn ACPI_GPE_DISPATCH_TYPE(flags: anytype) UINT8 {
    return @import("std").zig.c_translation.cast(UINT8, flags & ACPI_GPE_DISPATCH_MASK);
}
pub const ACPI_GPE_LEVEL_TRIGGERED = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x08));
pub const ACPI_GPE_EDGE_TRIGGERED = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x00));
pub const ACPI_GPE_XRUPT_TYPE_MASK = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x08));
pub const ACPI_GPE_CAN_WAKE = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x10));
pub const ACPI_GPE_AUTO_ENABLED = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x20));
pub const ACPI_GPE_INITIALIZED = @import("std").zig.c_translation.cast(UINT8, @as(c_int, 0x40));
pub const ACPI_NOT_ISR = @as(c_int, 0x1);
pub const ACPI_ISR = @as(c_int, 0x0);
pub const ACPI_SYSTEM_NOTIFY = @as(c_int, 0x1);
pub const ACPI_DEVICE_NOTIFY = @as(c_int, 0x2);
pub const ACPI_ALL_NOTIFY = ACPI_SYSTEM_NOTIFY | ACPI_DEVICE_NOTIFY;
pub const ACPI_MAX_NOTIFY_HANDLER_TYPE = @as(c_int, 0x3);
pub const ACPI_NUM_NOTIFY_TYPES = @as(c_int, 2);
pub const ACPI_MAX_SYS_NOTIFY = @as(c_int, 0x7F);
pub const ACPI_MAX_DEVICE_SPECIFIC_NOTIFY = @as(c_int, 0xBF);
pub const ACPI_SYSTEM_HANDLER_LIST = @as(c_int, 0);
pub const ACPI_DEVICE_HANDLER_LIST = @as(c_int, 1);
pub const ACPI_ADR_SPACE_SYSTEM_MEMORY = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 0));
pub const ACPI_ADR_SPACE_SYSTEM_IO = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 1));
pub const ACPI_ADR_SPACE_PCI_CONFIG = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 2));
pub const ACPI_ADR_SPACE_EC = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 3));
pub const ACPI_ADR_SPACE_SMBUS = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 4));
pub const ACPI_ADR_SPACE_CMOS = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 5));
pub const ACPI_ADR_SPACE_PCI_BAR_TARGET = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 6));
pub const ACPI_ADR_SPACE_IPMI = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 7));
pub const ACPI_ADR_SPACE_GPIO = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 8));
pub const ACPI_ADR_SPACE_GSBUS = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 9));
pub const ACPI_ADR_SPACE_PLATFORM_COMM = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 10));
pub const ACPI_ADR_SPACE_PLATFORM_RT = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 11));
pub const ACPI_NUM_PREDEFINED_REGIONS = @as(c_int, 12);
pub const ACPI_ADR_SPACE_DATA_TABLE = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 0x7E));
pub const ACPI_ADR_SPACE_FIXED_HARDWARE = @import("std").zig.c_translation.cast(ACPI_ADR_SPACE_TYPE, @as(c_int, 0x7F));
pub const ACPI_REG_DISCONNECT = @as(c_int, 0);
pub const ACPI_REG_CONNECT = @as(c_int, 1);
pub const ACPI_BITREG_TIMER_STATUS = @as(c_int, 0x00);
pub const ACPI_BITREG_BUS_MASTER_STATUS = @as(c_int, 0x01);
pub const ACPI_BITREG_GLOBAL_LOCK_STATUS = @as(c_int, 0x02);
pub const ACPI_BITREG_POWER_BUTTON_STATUS = @as(c_int, 0x03);
pub const ACPI_BITREG_SLEEP_BUTTON_STATUS = @as(c_int, 0x04);
pub const ACPI_BITREG_RT_CLOCK_STATUS = @as(c_int, 0x05);
pub const ACPI_BITREG_WAKE_STATUS = @as(c_int, 0x06);
pub const ACPI_BITREG_PCIEXP_WAKE_STATUS = @as(c_int, 0x07);
pub const ACPI_BITREG_TIMER_ENABLE = @as(c_int, 0x08);
pub const ACPI_BITREG_GLOBAL_LOCK_ENABLE = @as(c_int, 0x09);
pub const ACPI_BITREG_POWER_BUTTON_ENABLE = @as(c_int, 0x0A);
pub const ACPI_BITREG_SLEEP_BUTTON_ENABLE = @as(c_int, 0x0B);
pub const ACPI_BITREG_RT_CLOCK_ENABLE = @as(c_int, 0x0C);
pub const ACPI_BITREG_PCIEXP_WAKE_DISABLE = @as(c_int, 0x0D);
pub const ACPI_BITREG_SCI_ENABLE = @as(c_int, 0x0E);
pub const ACPI_BITREG_BUS_MASTER_RLD = @as(c_int, 0x0F);
pub const ACPI_BITREG_GLOBAL_LOCK_RELEASE = @as(c_int, 0x10);
pub const ACPI_BITREG_SLEEP_TYPE = @as(c_int, 0x11);
pub const ACPI_BITREG_SLEEP_ENABLE = @as(c_int, 0x12);
pub const ACPI_BITREG_ARB_DISABLE = @as(c_int, 0x13);
pub const ACPI_BITREG_MAX = @as(c_int, 0x13);
pub const ACPI_NUM_BITREG = ACPI_BITREG_MAX + @as(c_int, 1);
pub const ACPI_CLEAR_STATUS = @as(c_int, 1);
pub const ACPI_ENABLE_EVENT = @as(c_int, 1);
pub const ACPI_DISABLE_EVENT = @as(c_int, 0);
pub const ACPI_NO_BUFFER = @as(c_int, 0);
pub const ACPI_ALLOCATE_BUFFER = @import("std").zig.c_translation.cast(ACPI_SIZE, -@as(c_int, 1));
pub const ACPI_ALLOCATE_LOCAL_BUFFER = @import("std").zig.c_translation.cast(ACPI_SIZE, -@as(c_int, 2));
pub const ACPI_FULL_PATHNAME = @as(c_int, 0);
pub const ACPI_SINGLE_NAME = @as(c_int, 1);
pub const ACPI_FULL_PATHNAME_NO_TRAILING = @as(c_int, 2);
pub const ACPI_NAME_TYPE_MAX = @as(c_int, 2);
pub const ACPI_SYS_MODE_UNKNOWN = @as(c_int, 0x0000);
pub const ACPI_SYS_MODE_ACPI = @as(c_int, 0x0001);
pub const ACPI_SYS_MODE_LEGACY = @as(c_int, 0x0002);
pub const ACPI_SYS_MODES_MASK = @as(c_int, 0x0003);
pub const ACPI_EVENT_TYPE_GPE = @as(c_int, 0);
pub const ACPI_EVENT_TYPE_FIXED = @as(c_int, 1);
pub const ACPI_INIT_DEVICE_INI = @as(c_int, 1);
pub const ACPI_TABLE_EVENT_LOAD = @as(c_int, 0x0);
pub const ACPI_TABLE_EVENT_UNLOAD = @as(c_int, 0x1);
pub const ACPI_TABLE_EVENT_INSTALL = @as(c_int, 0x2);
pub const ACPI_TABLE_EVENT_UNINSTALL = @as(c_int, 0x3);
pub const ACPI_NUM_TABLE_EVENTS = @as(c_int, 4);
pub const ACPI_DEFAULT_HANDLER = NULL;
pub const ACPI_REGION_ACTIVATE = @as(c_int, 0);
pub const ACPI_REGION_DEACTIVATE = @as(c_int, 1);
pub const ACPI_INTERRUPT_NOT_HANDLED = @as(c_int, 0x00);
pub const ACPI_INTERRUPT_HANDLED = @as(c_int, 0x01);
pub const ACPI_REENABLE_GPE = @as(c_int, 0x80);
pub const ACPI_EISAID_STRING_SIZE = @as(c_int, 8);
pub const ACPI_UUID_LENGTH = @as(c_int, 16);
pub const ACPI_PCICLS_STRING_SIZE = @as(c_int, 7);
pub const ACPI_PCI_ROOT_BRIDGE = @as(c_int, 0x01);
pub const ACPI_VALID_ADR = @as(c_int, 0x0002);
pub const ACPI_VALID_HID = @as(c_int, 0x0004);
pub const ACPI_VALID_UID = @as(c_int, 0x0008);
pub const ACPI_VALID_CID = @as(c_int, 0x0020);
pub const ACPI_VALID_CLS = @as(c_int, 0x0040);
pub const ACPI_VALID_SXDS = @as(c_int, 0x0100);
pub const ACPI_VALID_SXWS = @as(c_int, 0x0200);
pub const ACPI_STA_DEVICE_PRESENT = @as(c_int, 0x01);
pub const ACPI_STA_DEVICE_ENABLED = @as(c_int, 0x02);
pub const ACPI_STA_DEVICE_UI = @as(c_int, 0x04);
pub const ACPI_STA_DEVICE_FUNCTIONING = @as(c_int, 0x08);
pub const ACPI_STA_DEVICE_OK = @as(c_int, 0x08);
pub const ACPI_STA_BATTERY_PRESENT = @as(c_int, 0x10);
pub const ACPI_VENDOR_STRINGS = @as(c_int, 0x01);
pub const ACPI_FEATURE_STRINGS = @as(c_int, 0x02);
pub const ACPI_ENABLE_INTERFACES = @as(c_int, 0x00);
pub const ACPI_DISABLE_INTERFACES = @as(c_int, 0x04);
pub const ACPI_DISABLE_ALL_VENDOR_STRINGS = ACPI_DISABLE_INTERFACES | ACPI_VENDOR_STRINGS;
pub const ACPI_DISABLE_ALL_FEATURE_STRINGS = ACPI_DISABLE_INTERFACES | ACPI_FEATURE_STRINGS;
pub const ACPI_DISABLE_ALL_STRINGS = (ACPI_DISABLE_INTERFACES | ACPI_VENDOR_STRINGS) | ACPI_FEATURE_STRINGS;
pub const ACPI_ENABLE_ALL_VENDOR_STRINGS = ACPI_ENABLE_INTERFACES | ACPI_VENDOR_STRINGS;
pub const ACPI_ENABLE_ALL_FEATURE_STRINGS = ACPI_ENABLE_INTERFACES | ACPI_FEATURE_STRINGS;
pub const ACPI_ENABLE_ALL_STRINGS = (ACPI_ENABLE_INTERFACES | ACPI_VENDOR_STRINGS) | ACPI_FEATURE_STRINGS;
pub const ACPI_OSI_WIN_2000 = @as(c_int, 0x01);
pub const ACPI_OSI_WIN_XP = @as(c_int, 0x02);
pub const ACPI_OSI_WIN_XP_SP1 = @as(c_int, 0x03);
pub const ACPI_OSI_WINSRV_2003 = @as(c_int, 0x04);
pub const ACPI_OSI_WIN_XP_SP2 = @as(c_int, 0x05);
pub const ACPI_OSI_WINSRV_2003_SP1 = @as(c_int, 0x06);
pub const ACPI_OSI_WIN_VISTA = @as(c_int, 0x07);
pub const ACPI_OSI_WINSRV_2008 = @as(c_int, 0x08);
pub const ACPI_OSI_WIN_VISTA_SP1 = @as(c_int, 0x09);
pub const ACPI_OSI_WIN_VISTA_SP2 = @as(c_int, 0x0A);
pub const ACPI_OSI_WIN_7 = @as(c_int, 0x0B);
pub const ACPI_OSI_WIN_8 = @as(c_int, 0x0C);
pub const ACPI_OSI_WIN_8_1 = @as(c_int, 0x0D);
pub const ACPI_OSI_WIN_10 = @as(c_int, 0x0E);
pub const ACPI_OSI_WIN_10_RS1 = @as(c_int, 0x0F);
pub const ACPI_OSI_WIN_10_RS2 = @as(c_int, 0x10);
pub const ACPI_OSI_WIN_10_RS3 = @as(c_int, 0x11);
pub const ACPI_OSI_WIN_10_RS4 = @as(c_int, 0x12);
pub const ACPI_OSI_WIN_10_RS5 = @as(c_int, 0x13);
pub const ACPI_OSI_WIN_10_19H1 = @as(c_int, 0x14);
pub const ACPI_OSI_WIN_10_20H1 = @as(c_int, 0x15);
pub const ACPI_OSI_WIN_11 = @as(c_int, 0x16);
pub const ACPI_OPT_END = -@as(c_int, 1);
pub const __ANIVA_ACPI_TABLES__ = "";
pub const __ACTBL_H__ = "";
pub const ACPI_SIG_DSDT = "DSDT";
pub const ACPI_SIG_FADT = "FACP";
pub const ACPI_SIG_FACS = "FACS";
pub const ACPI_SIG_OSDT = "OSDT";
pub const ACPI_SIG_PSDT = "PSDT";
pub const ACPI_SIG_RSDP = "RSD PTR ";
pub const ACPI_SIG_RSDT = "RSDT";
pub const ACPI_SIG_XSDT = "XSDT";
pub const ACPI_SIG_SSDT = "SSDT";
pub const ACPI_RSDP_NAME = "RSDP";
pub const ACPI_OEM_NAME = "OEM";
pub const ACPI_RSDT_ENTRY_SIZE = @import("std").zig.c_translation.sizeof(UINT32);
pub const ACPI_XSDT_ENTRY_SIZE = @import("std").zig.c_translation.sizeof(UINT64);
pub const ACPI_GLOCK_PENDING = @as(c_int, 1);
pub const ACPI_GLOCK_OWNED = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_FACS_S4_BIOS_PRESENT = @as(c_int, 1);
pub const ACPI_FACS_64BIT_WAKE = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_FACS_64BIT_ENVIRONMENT = @as(c_int, 1);
pub const ACPI_FADT_LEGACY_DEVICES = @as(c_int, 1);
pub const ACPI_FADT_8042 = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_FADT_NO_VGA = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_FADT_NO_MSI = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_FADT_NO_ASPM = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_FADT_NO_CMOS_RTC = @as(c_int, 1) << @as(c_int, 5);
pub const ACPI_FADT_PSCI_COMPLIANT = @as(c_int, 1);
pub const ACPI_FADT_PSCI_USE_HVC = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_FADT_WBINVD = @as(c_int, 1);
pub const ACPI_FADT_WBINVD_FLUSH = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_FADT_C1_SUPPORTED = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_FADT_C2_MP_SUPPORTED = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_FADT_POWER_BUTTON = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_FADT_SLEEP_BUTTON = @as(c_int, 1) << @as(c_int, 5);
pub const ACPI_FADT_FIXED_RTC = @as(c_int, 1) << @as(c_int, 6);
pub const ACPI_FADT_S4_RTC_WAKE = @as(c_int, 1) << @as(c_int, 7);
pub const ACPI_FADT_32BIT_TIMER = @as(c_int, 1) << @as(c_int, 8);
pub const ACPI_FADT_DOCKING_SUPPORTED = @as(c_int, 1) << @as(c_int, 9);
pub const ACPI_FADT_RESET_REGISTER = @as(c_int, 1) << @as(c_int, 10);
pub const ACPI_FADT_SEALED_CASE = @as(c_int, 1) << @as(c_int, 11);
pub const ACPI_FADT_HEADLESS = @as(c_int, 1) << @as(c_int, 12);
pub const ACPI_FADT_SLEEP_TYPE = @as(c_int, 1) << @as(c_int, 13);
pub const ACPI_FADT_PCI_EXPRESS_WAKE = @as(c_int, 1) << @as(c_int, 14);
pub const ACPI_FADT_PLATFORM_CLOCK = @as(c_int, 1) << @as(c_int, 15);
pub const ACPI_FADT_S4_RTC_VALID = @as(c_int, 1) << @as(c_int, 16);
pub const ACPI_FADT_REMOTE_POWER_ON = @as(c_int, 1) << @as(c_int, 17);
pub const ACPI_FADT_APIC_CLUSTER = @as(c_int, 1) << @as(c_int, 18);
pub const ACPI_FADT_APIC_PHYSICAL = @as(c_int, 1) << @as(c_int, 19);
pub const ACPI_FADT_HW_REDUCED = @as(c_int, 1) << @as(c_int, 20);
pub const ACPI_FADT_LOW_POWER_S0 = @as(c_int, 1) << @as(c_int, 21);
pub const ACPI_X_WAKE_STATUS = @as(c_int, 0x80);
pub const ACPI_X_SLEEP_TYPE_MASK = @as(c_int, 0x1C);
pub const ACPI_X_SLEEP_TYPE_POSITION = @as(c_int, 0x02);
pub const ACPI_X_SLEEP_ENABLE = @as(c_int, 0x20);
pub const ACPI_MAX_TABLE_VALIDATIONS = ACPI_UINT16_MAX;
pub const ACPI_TABLE_ORIGIN_EXTERNAL_VIRTUAL = @as(c_int, 0);
pub const ACPI_TABLE_ORIGIN_INTERNAL_PHYSICAL = @as(c_int, 1);
pub const ACPI_TABLE_ORIGIN_INTERNAL_VIRTUAL = @as(c_int, 2);
pub const ACPI_TABLE_ORIGIN_MASK = @as(c_int, 3);
pub const ACPI_TABLE_IS_VERIFIED = @as(c_int, 4);
pub const ACPI_TABLE_IS_LOADED = @as(c_int, 8);
pub const __ACTBL1_H__ = "";
pub const ACPI_SIG_AEST = "AEST";
pub const ACPI_SIG_ASF = "ASF!";
pub const ACPI_SIG_ASPT = "ASPT";
pub const ACPI_SIG_BERT = "BERT";
pub const ACPI_SIG_BGRT = "BGRT";
pub const ACPI_SIG_BOOT = "BOOT";
pub const ACPI_SIG_CEDT = "CEDT";
pub const ACPI_SIG_CPEP = "CPEP";
pub const ACPI_SIG_CSRT = "CSRT";
pub const ACPI_SIG_DBG2 = "DBG2";
pub const ACPI_SIG_DBGP = "DBGP";
pub const ACPI_SIG_DMAR = "DMAR";
pub const ACPI_SIG_DRTM = "DRTM";
pub const ACPI_SIG_ECDT = "ECDT";
pub const ACPI_SIG_EINJ = "EINJ";
pub const ACPI_SIG_ERST = "ERST";
pub const ACPI_SIG_FPDT = "FPDT";
pub const ACPI_SIG_GTDT = "GTDT";
pub const ACPI_SIG_HEST = "HEST";
pub const ACPI_SIG_HMAT = "HMAT";
pub const ACPI_SIG_HPET = "HPET";
pub const ACPI_SIG_IBFT = "IBFT";
pub const ACPI_SIG_MSCT = "MSCT";
pub const ACPI_SIG_S3PT = "S3PT";
pub const ACPI_SIG_PCCS = "PCC";
pub const ACPI_SIG_MATR = "MATR";
pub const ACPI_SIG_MSDM = "MSDM";
pub const ACPI_ASF_SMBUS_PROTOCOLS = @as(c_int, 1);
pub const ACPI_BERT_UNCORRECTABLE = @as(c_int, 1);
pub const ACPI_BERT_CORRECTABLE = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_BERT_MULTIPLE_UNCORRECTABLE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_BERT_MULTIPLE_CORRECTABLE = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_BERT_ERROR_ENTRY_COUNT = @as(c_int, 0xFF) << @as(c_int, 4);
pub const ACPI_BGRT_DISPLAYED = @as(c_int, 1);
pub const ACPI_BGRT_ORIENTATION_OFFSET = @as(c_int, 3) << @as(c_int, 1);
pub const ACPI_CDAT_DSMAS_NON_VOLATILE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_CDAT_DSIS_MEM_ATTACHED = @as(c_int, 1) << @as(c_int, 0);
pub const ACPI_CDAT_SSLBIS_US_PORT = @as(c_int, 0x0100);
pub const ACPI_CDAT_SSLBIS_ANY_PORT = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xffff, .hexadecimal);
pub const ACPI_CEDT_CHBS_VERSION_CXL11 = @as(c_int, 0);
pub const ACPI_CEDT_CHBS_VERSION_CXL20 = @as(c_int, 1);
pub const ACPI_CEDT_CHBS_LENGTH_CXL11 = @as(c_int, 0x2000);
pub const ACPI_CEDT_CHBS_LENGTH_CXL20 = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x10000, .hexadecimal);
pub const ACPI_CEDT_CFMWS_ARITHMETIC_MODULO = @as(c_int, 0);
pub const ACPI_CEDT_CFMWS_ARITHMETIC_XOR = @as(c_int, 1);
pub const ACPI_CEDT_CFMWS_RESTRICT_TYPE2 = @as(c_int, 1);
pub const ACPI_CEDT_CFMWS_RESTRICT_TYPE3 = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_CEDT_CFMWS_RESTRICT_VOLATILE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_CEDT_CFMWS_RESTRICT_PMEM = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_CEDT_CFMWS_RESTRICT_FIXED = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_CEDT_RDPAS_BUS_MASK = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xff00, .hexadecimal);
pub const ACPI_CEDT_RDPAS_DEVICE_MASK = @as(c_int, 0x00f8);
pub const ACPI_CEDT_RDPAS_FUNCTION_MASK = @as(c_int, 0x0007);
pub const ACPI_CEDT_RDPAS_PROTOCOL_IO = @as(c_int, 0);
pub const ACPI_CEDT_RDPAS_PROTOCOL_CACHEMEM = @as(c_int, 1);
pub const ACPI_CSRT_TYPE_INTERRUPT = @as(c_int, 0x0001);
pub const ACPI_CSRT_TYPE_TIMER = @as(c_int, 0x0002);
pub const ACPI_CSRT_TYPE_DMA = @as(c_int, 0x0003);
pub const ACPI_CSRT_XRUPT_LINE = @as(c_int, 0x0000);
pub const ACPI_CSRT_XRUPT_CONTROLLER = @as(c_int, 0x0001);
pub const ACPI_CSRT_TIMER = @as(c_int, 0x0000);
pub const ACPI_CSRT_DMA_CHANNEL = @as(c_int, 0x0000);
pub const ACPI_CSRT_DMA_CONTROLLER = @as(c_int, 0x0001);
pub const ACPI_DBG2_SERIAL_PORT = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x8000, .hexadecimal);
pub const ACPI_DBG2_1394_PORT = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x8001, .hexadecimal);
pub const ACPI_DBG2_USB_PORT = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x8002, .hexadecimal);
pub const ACPI_DBG2_NET_PORT = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x8003, .hexadecimal);
pub const ACPI_DBG2_16550_COMPATIBLE = @as(c_int, 0x0000);
pub const ACPI_DBG2_16550_SUBSET = @as(c_int, 0x0001);
pub const ACPI_DBG2_MAX311XE_SPI = @as(c_int, 0x0002);
pub const ACPI_DBG2_ARM_PL011 = @as(c_int, 0x0003);
pub const ACPI_DBG2_MSM8X60 = @as(c_int, 0x0004);
pub const ACPI_DBG2_16550_NVIDIA = @as(c_int, 0x0005);
pub const ACPI_DBG2_TI_OMAP = @as(c_int, 0x0006);
pub const ACPI_DBG2_APM88XXXX = @as(c_int, 0x0008);
pub const ACPI_DBG2_MSM8974 = @as(c_int, 0x0009);
pub const ACPI_DBG2_SAM5250 = @as(c_int, 0x000A);
pub const ACPI_DBG2_INTEL_USIF = @as(c_int, 0x000B);
pub const ACPI_DBG2_IMX6 = @as(c_int, 0x000C);
pub const ACPI_DBG2_ARM_SBSA_32BIT = @as(c_int, 0x000D);
pub const ACPI_DBG2_ARM_SBSA_GENERIC = @as(c_int, 0x000E);
pub const ACPI_DBG2_ARM_DCC = @as(c_int, 0x000F);
pub const ACPI_DBG2_BCM2835 = @as(c_int, 0x0010);
pub const ACPI_DBG2_SDM845_1_8432MHZ = @as(c_int, 0x0011);
pub const ACPI_DBG2_16550_WITH_GAS = @as(c_int, 0x0012);
pub const ACPI_DBG2_SDM845_7_372MHZ = @as(c_int, 0x0013);
pub const ACPI_DBG2_INTEL_LPSS = @as(c_int, 0x0014);
pub const ACPI_DBG2_1394_STANDARD = @as(c_int, 0x0000);
pub const ACPI_DBG2_USB_XHCI = @as(c_int, 0x0000);
pub const ACPI_DBG2_USB_EHCI = @as(c_int, 0x0001);
pub const ACPI_DMAR_INTR_REMAP = @as(c_int, 1);
pub const ACPI_DMAR_X2APIC_OPT_OUT = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_DMAR_X2APIC_MODE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_DMAR_INCLUDE_ALL = @as(c_int, 1);
pub const ACPI_DMAR_ALLOW_ALL = @as(c_int, 1);
pub const ACPI_DMAR_ALL_PORTS = @as(c_int, 1);
pub const ACPI_DRTM_ACCESS_ALLOWED = @as(c_int, 1);
pub const ACPI_DRTM_ENABLE_GAP_CODE = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_DRTM_INCOMPLETE_MEASUREMENTS = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_DRTM_AUTHORITY_ORDER = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_EINJ_PRESERVE = @as(c_int, 1);
pub const ACPI_EINJ_PROCESSOR_CORRECTABLE = @as(c_int, 1);
pub const ACPI_EINJ_PROCESSOR_UNCORRECTABLE = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_EINJ_PROCESSOR_FATAL = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_EINJ_MEMORY_CORRECTABLE = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_EINJ_MEMORY_UNCORRECTABLE = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_EINJ_MEMORY_FATAL = @as(c_int, 1) << @as(c_int, 5);
pub const ACPI_EINJ_PCIX_CORRECTABLE = @as(c_int, 1) << @as(c_int, 6);
pub const ACPI_EINJ_PCIX_UNCORRECTABLE = @as(c_int, 1) << @as(c_int, 7);
pub const ACPI_EINJ_PCIX_FATAL = @as(c_int, 1) << @as(c_int, 8);
pub const ACPI_EINJ_PLATFORM_CORRECTABLE = @as(c_int, 1) << @as(c_int, 9);
pub const ACPI_EINJ_PLATFORM_UNCORRECTABLE = @as(c_int, 1) << @as(c_int, 10);
pub const ACPI_EINJ_PLATFORM_FATAL = @as(c_int, 1) << @as(c_int, 11);
pub const ACPI_EINJ_CXL_CACHE_CORRECTABLE = @as(c_int, 1) << @as(c_int, 12);
pub const ACPI_EINJ_CXL_CACHE_UNCORRECTABLE = @as(c_int, 1) << @as(c_int, 13);
pub const ACPI_EINJ_CXL_CACHE_FATAL = @as(c_int, 1) << @as(c_int, 14);
pub const ACPI_EINJ_CXL_MEM_CORRECTABLE = @as(c_int, 1) << @as(c_int, 15);
pub const ACPI_EINJ_CXL_MEM_UNCORRECTABLE = @as(c_int, 1) << @as(c_int, 16);
pub const ACPI_EINJ_CXL_MEM_FATAL = @as(c_int, 1) << @as(c_int, 17);
pub const ACPI_EINJ_VENDOR_DEFINED = @as(c_int, 1) << @as(c_int, 31);
pub const ACPI_ERST_PRESERVE = @as(c_int, 1);
pub const ACPI_GTDT_INTERRUPT_MODE = @as(c_int, 1);
pub const ACPI_GTDT_INTERRUPT_POLARITY = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_GTDT_ALWAYS_ON = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_GTDT_GT_IRQ_MODE = @as(c_int, 1);
pub const ACPI_GTDT_GT_IRQ_POLARITY = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_GTDT_GT_IS_SECURE_TIMER = @as(c_int, 1);
pub const ACPI_GTDT_GT_ALWAYS_ON = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_GTDT_WATCHDOG_IRQ_MODE = @as(c_int, 1);
pub const ACPI_GTDT_WATCHDOG_IRQ_POLARITY = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_GTDT_WATCHDOG_SECURE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_HEST_FIRMWARE_FIRST = @as(c_int, 1);
pub const ACPI_HEST_GLOBAL = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_HEST_GHES_ASSIST = @as(c_int, 1) << @as(c_int, 2);
pub inline fn ACPI_HEST_BUS(Bus: anytype) @TypeOf(Bus & @as(c_int, 0xFF)) {
    return Bus & @as(c_int, 0xFF);
}
pub inline fn ACPI_HEST_SEGMENT(Bus: anytype) @TypeOf((Bus >> @as(c_int, 8)) & @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xFFFF, .hexadecimal)) {
    return (Bus >> @as(c_int, 8)) & @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xFFFF, .hexadecimal);
}
pub const ACPI_HEST_TYPE = @as(c_int, 1);
pub const ACPI_HEST_POLL_INTERVAL = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_HEST_POLL_THRESHOLD_VALUE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_HEST_POLL_THRESHOLD_WINDOW = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_HEST_ERR_THRESHOLD_VALUE = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_HEST_ERR_THRESHOLD_WINDOW = @as(c_int, 1) << @as(c_int, 5);
pub const ACPI_HEST_UNCORRECTABLE = @as(c_int, 1);
pub const ACPI_HEST_CORRECTABLE = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_HEST_MULTIPLE_UNCORRECTABLE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_HEST_MULTIPLE_CORRECTABLE = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_HEST_ERROR_ENTRY_COUNT = @as(c_int, 0xFF) << @as(c_int, 4);
pub const ACPI_HEST_GEN_ERROR_RECOVERABLE = @as(c_int, 0);
pub const ACPI_HEST_GEN_ERROR_FATAL = @as(c_int, 1);
pub const ACPI_HEST_GEN_ERROR_CORRECTED = @as(c_int, 2);
pub const ACPI_HEST_GEN_ERROR_NONE = @as(c_int, 3);
pub const ACPI_HEST_GEN_VALID_FRU_ID = @as(c_int, 1);
pub const ACPI_HEST_GEN_VALID_FRU_STRING = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_HEST_GEN_VALID_TIMESTAMP = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_HMAT_INITIATOR_PD_VALID = @as(c_int, 1);
pub const ACPI_HMAT_MEMORY_HIERARCHY = @as(c_int, 0x0F);
pub const ACPI_HMAT_MEMORY = @as(c_int, 0);
pub const ACPI_HMAT_1ST_LEVEL_CACHE = @as(c_int, 1);
pub const ACPI_HMAT_2ND_LEVEL_CACHE = @as(c_int, 2);
pub const ACPI_HMAT_3RD_LEVEL_CACHE = @as(c_int, 3);
pub const ACPI_HMAT_MINIMUM_XFER_SIZE = @as(c_int, 0x10);
pub const ACPI_HMAT_NON_SEQUENTIAL_XFERS = @as(c_int, 0x20);
pub const ACPI_HMAT_ACCESS_LATENCY = @as(c_int, 0);
pub const ACPI_HMAT_READ_LATENCY = @as(c_int, 1);
pub const ACPI_HMAT_WRITE_LATENCY = @as(c_int, 2);
pub const ACPI_HMAT_ACCESS_BANDWIDTH = @as(c_int, 3);
pub const ACPI_HMAT_READ_BANDWIDTH = @as(c_int, 4);
pub const ACPI_HMAT_WRITE_BANDWIDTH = @as(c_int, 5);
pub const ACPI_HMAT_TOTAL_CACHE_LEVEL = @as(c_int, 0x0000000F);
pub const ACPI_HMAT_CACHE_LEVEL = @as(c_int, 0x000000F0);
pub const ACPI_HMAT_CACHE_ASSOCIATIVITY = @as(c_int, 0x00000F00);
pub const ACPI_HMAT_WRITE_POLICY = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x0000F000, .hexadecimal);
pub const ACPI_HMAT_CACHE_LINE_SIZE = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xFFFF0000, .hexadecimal);
pub const ACPI_HMAT_CA_NONE = @as(c_int, 0);
pub const ACPI_HMAT_CA_DIRECT_MAPPED = @as(c_int, 1);
pub const ACPI_HMAT_CA_COMPLEX_CACHE_INDEXING = @as(c_int, 2);
pub const ACPI_HMAT_CP_NONE = @as(c_int, 0);
pub const ACPI_HMAT_CP_WB = @as(c_int, 1);
pub const ACPI_HMAT_CP_WT = @as(c_int, 2);
pub const ACPI_HPET_PAGE_PROTECT_MASK = @as(c_int, 3);
pub const __ACTBL2_H__ = "";
pub const ACPI_SIG_AGDI = "AGDI";
pub const ACPI_SIG_APMT = "APMT";
pub const ACPI_SIG_BDAT = "BDAT";
pub const ACPI_SIG_CCEL = "CCEL";
pub const ACPI_SIG_CDAT = "CDAT";
pub const ACPI_SIG_IORT = "IORT";
pub const ACPI_SIG_IVRS = "IVRS";
pub const ACPI_SIG_LPIT = "LPIT";
pub const ACPI_SIG_MADT = "APIC";
pub const ACPI_SIG_MCFG = "MCFG";
pub const ACPI_SIG_MCHI = "MCHI";
pub const ACPI_SIG_MPAM = "MPAM";
pub const ACPI_SIG_MPST = "MPST";
pub const ACPI_SIG_NFIT = "NFIT";
pub const ACPI_SIG_NHLT = "NHLT";
pub const ACPI_SIG_PCCT = "PCCT";
pub const ACPI_SIG_PDTT = "PDTT";
pub const ACPI_SIG_PHAT = "PHAT";
pub const ACPI_SIG_PMTT = "PMTT";
pub const ACPI_SIG_PPTT = "PPTT";
pub const ACPI_SIG_PRMT = "PRMT";
pub const ACPI_SIG_RASF = "RASF";
pub const ACPI_SIG_RAS2 = "RAS2";
pub const ACPI_SIG_RGRT = "RGRT";
pub const ACPI_SIG_RHCT = "RHCT";
pub const ACPI_SIG_SBST = "SBST";
pub const ACPI_SIG_SDEI = "SDEI";
pub const ACPI_SIG_SDEV = "SDEV";
pub const ACPI_SIG_SVKL = "SVKL";
pub const ACPI_SIG_TDEL = "TDEL";
pub const ACPI_AEST_PROCESSOR_ERROR_NODE = @as(c_int, 0);
pub const ACPI_AEST_MEMORY_ERROR_NODE = @as(c_int, 1);
pub const ACPI_AEST_SMMU_ERROR_NODE = @as(c_int, 2);
pub const ACPI_AEST_VENDOR_ERROR_NODE = @as(c_int, 3);
pub const ACPI_AEST_GIC_ERROR_NODE = @as(c_int, 4);
pub const ACPI_AEST_NODE_TYPE_RESERVED = @as(c_int, 5);
pub const ACPI_AEST_CACHE_RESOURCE = @as(c_int, 0);
pub const ACPI_AEST_TLB_RESOURCE = @as(c_int, 1);
pub const ACPI_AEST_GENERIC_RESOURCE = @as(c_int, 2);
pub const ACPI_AEST_RESOURCE_RESERVED = @as(c_int, 3);
pub const ACPI_AEST_CACHE_DATA = @as(c_int, 0);
pub const ACPI_AEST_CACHE_INSTRUCTION = @as(c_int, 1);
pub const ACPI_AEST_CACHE_UNIFIED = @as(c_int, 2);
pub const ACPI_AEST_CACHE_RESERVED = @as(c_int, 3);
pub const ACPI_AEST_GIC_CPU = @as(c_int, 0);
pub const ACPI_AEST_GIC_DISTRIBUTOR = @as(c_int, 1);
pub const ACPI_AEST_GIC_REDISTRIBUTOR = @as(c_int, 2);
pub const ACPI_AEST_GIC_ITS = @as(c_int, 3);
pub const ACPI_AEST_GIC_RESERVED = @as(c_int, 4);
pub const ACPI_AEST_NODE_SYSTEM_REGISTER = @as(c_int, 0);
pub const ACPI_AEST_NODE_MEMORY_MAPPED = @as(c_int, 1);
pub const ACPI_AEST_XFACE_RESERVED = @as(c_int, 2);
pub const ACPI_AEST_NODE_FAULT_HANDLING = @as(c_int, 0);
pub const ACPI_AEST_NODE_ERROR_RECOVERY = @as(c_int, 1);
pub const ACPI_AEST_XRUPT_RESERVED = @as(c_int, 2);
pub const ACPI_AGDI_SIGNALING_MODE = @as(c_int, 1);
pub const ACPI_APMT_NODE_ID_LENGTH = @as(c_int, 4);
pub const ACPI_APMT_FLAGS_DUAL_PAGE = @as(c_int, 1) << @as(c_int, 0);
pub const ACPI_APMT_FLAGS_AFFINITY = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_APMT_FLAGS_ATOMIC = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_APMT_FLAGS_DUAL_PAGE_NSUPP = @as(c_int, 0) << @as(c_int, 0);
pub const ACPI_APMT_FLAGS_DUAL_PAGE_SUPP = @as(c_int, 1) << @as(c_int, 0);
pub const ACPI_APMT_FLAGS_AFFINITY_PROC = @as(c_int, 0) << @as(c_int, 1);
pub const ACPI_APMT_FLAGS_AFFINITY_PROC_CONTAINER = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_APMT_FLAGS_ATOMIC_NSUPP = @as(c_int, 0) << @as(c_int, 2);
pub const ACPI_APMT_FLAGS_ATOMIC_SUPP = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_APMT_OVFLW_IRQ_FLAGS_MODE = @as(c_int, 1) << @as(c_int, 0);
pub const ACPI_APMT_OVFLW_IRQ_FLAGS_TYPE = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_APMT_OVFLW_IRQ_FLAGS_MODE_LEVEL = @as(c_int, 0) << @as(c_int, 0);
pub const ACPI_APMT_OVFLW_IRQ_FLAGS_MODE_EDGE = @as(c_int, 1) << @as(c_int, 0);
pub const ACPI_APMT_OVFLW_IRQ_FLAGS_TYPE_WIRED = @as(c_int, 0) << @as(c_int, 1);
pub const ACPI_IORT_ID_SINGLE_MAPPING = @as(c_int, 1);
pub const ACPI_IORT_NODE_COHERENT = @as(c_int, 0x00000001);
pub const ACPI_IORT_NODE_NOT_COHERENT = @as(c_int, 0x00000000);
pub const ACPI_IORT_HT_TRANSIENT = @as(c_int, 1);
pub const ACPI_IORT_HT_WRITE = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_IORT_HT_READ = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_IORT_HT_OVERRIDE = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_IORT_MF_COHERENCY = @as(c_int, 1);
pub const ACPI_IORT_MF_ATTRIBUTES = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_IORT_NC_STALL_SUPPORTED = @as(c_int, 1);
pub const ACPI_IORT_NC_PASID_BITS = @as(c_int, 31) << @as(c_int, 1);
pub const ACPI_IORT_ATS_SUPPORTED = @as(c_int, 1);
pub const ACPI_IORT_PRI_SUPPORTED = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_IORT_PASID_FWD_SUPPORTED = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_IORT_PASID_MAX_WIDTH = @as(c_int, 0x1F);
pub const ACPI_IORT_SMMU_V1 = @as(c_int, 0x00000000);
pub const ACPI_IORT_SMMU_V2 = @as(c_int, 0x00000001);
pub const ACPI_IORT_SMMU_CORELINK_MMU400 = @as(c_int, 0x00000002);
pub const ACPI_IORT_SMMU_CORELINK_MMU500 = @as(c_int, 0x00000003);
pub const ACPI_IORT_SMMU_CORELINK_MMU401 = @as(c_int, 0x00000004);
pub const ACPI_IORT_SMMU_CAVIUM_THUNDERX = @as(c_int, 0x00000005);
pub const ACPI_IORT_SMMU_DVM_SUPPORTED = @as(c_int, 1);
pub const ACPI_IORT_SMMU_COHERENT_WALK = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_IORT_SMMU_V3_GENERIC = @as(c_int, 0x00000000);
pub const ACPI_IORT_SMMU_V3_HISILICON_HI161X = @as(c_int, 0x00000001);
pub const ACPI_IORT_SMMU_V3_CAVIUM_CN99XX = @as(c_int, 0x00000002);
pub const ACPI_IORT_SMMU_V3_COHACC_OVERRIDE = @as(c_int, 1);
pub const ACPI_IORT_SMMU_V3_HTTU_OVERRIDE = @as(c_int, 3) << @as(c_int, 1);
pub const ACPI_IORT_SMMU_V3_PXM_VALID = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_IORT_SMMU_V3_DEVICEID_VALID = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_IORT_RMR_REMAP_PERMITTED = @as(c_int, 1);
pub const ACPI_IORT_RMR_ACCESS_PRIVILEGE = @as(c_int, 1) << @as(c_int, 1);
pub inline fn ACPI_IORT_RMR_ACCESS_ATTRIBUTES(flags: anytype) @TypeOf((flags >> @as(c_int, 2)) & @as(c_int, 0xFF)) {
    return (flags >> @as(c_int, 2)) & @as(c_int, 0xFF);
}
pub const ACPI_IORT_RMR_ATTR_DEVICE_NGNRNE = @as(c_int, 0x00);
pub const ACPI_IORT_RMR_ATTR_DEVICE_NGNRE = @as(c_int, 0x01);
pub const ACPI_IORT_RMR_ATTR_DEVICE_NGRE = @as(c_int, 0x02);
pub const ACPI_IORT_RMR_ATTR_DEVICE_GRE = @as(c_int, 0x03);
pub const ACPI_IORT_RMR_ATTR_NORMAL_NC = @as(c_int, 0x04);
pub const ACPI_IORT_RMR_ATTR_NORMAL_IWB_OWB = @as(c_int, 0x05);
pub const ACPI_IVRS_PHYSICAL_SIZE = @as(c_int, 0x00007F00);
pub const ACPI_IVRS_VIRTUAL_SIZE = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x003F8000, .hexadecimal);
pub const ACPI_IVRS_ATS_RESERVED = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x00400000, .hexadecimal);
pub const ACPI_IVHD_TT_ENABLE = @as(c_int, 1);
pub const ACPI_IVHD_PASS_PW = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_IVHD_RES_PASS_PW = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_IVHD_ISOC = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_IVHD_IOTLB = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_IVMD_UNITY = @as(c_int, 1);
pub const ACPI_IVMD_READ = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_IVMD_WRITE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_IVMD_EXCLUSION_RANGE = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_IVHD_MSI_NUMBER_MASK = @as(c_int, 0x001F);
pub const ACPI_IVHD_UNIT_ID_MASK = @as(c_int, 0x1F00);
pub const ACPI_IVHD_ENTRY_LENGTH = @as(c_int, 0xC0);
pub const ACPI_IVHD_INIT_PASS = @as(c_int, 1);
pub const ACPI_IVHD_EINT_PASS = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_IVHD_NMI_PASS = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_IVHD_SYSTEM_MGMT = @as(c_int, 3) << @as(c_int, 4);
pub const ACPI_IVHD_LINT0_PASS = @as(c_int, 1) << @as(c_int, 6);
pub const ACPI_IVHD_LINT1_PASS = @as(c_int, 1) << @as(c_int, 7);
pub const ACPI_IVHD_ATS_DISABLED = @as(c_int, 1) << @as(c_int, 31);
pub const ACPI_IVHD_IOAPIC = @as(c_int, 1);
pub const ACPI_IVHD_HPET = @as(c_int, 2);
pub const ACPI_IVRS_UID_NOT_PRESENT = @as(c_int, 0);
pub const ACPI_IVRS_UID_IS_INTEGER = @as(c_int, 1);
pub const ACPI_IVRS_UID_IS_STRING = @as(c_int, 2);
pub const ACPI_LPIT_STATE_DISABLED = @as(c_int, 1);
pub const ACPI_LPIT_NO_COUNTER = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_MADT_PCAT_COMPAT = @as(c_int, 1);
pub const ACPI_MADT_DUAL_PIC = @as(c_int, 1);
pub const ACPI_MADT_MULTIPLE_APIC = @as(c_int, 0);
pub const ACPI_MADT_CPEI_OVERRIDE = @as(c_int, 1);
pub const ACPI_MADT_PERFORMANCE_IRQ_MODE = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_MADT_VGIC_IRQ_MODE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_MADT_GICC_ONLINE_CAPABLE = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_MADT_GICC_NON_COHERENT = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_MADT_OVERRIDE_SPI_VALUES = @as(c_int, 1);
pub const ACPI_MADT_GICR_NON_COHERENT = @as(c_int, 1);
pub const ACPI_MADT_ITS_NON_COHERENT = @as(c_int, 1);
pub const ACPI_MULTIPROC_WAKEUP_MB_OS_SIZE = @as(c_int, 2032);
pub const ACPI_MULTIPROC_WAKEUP_MB_FIRMWARE_SIZE = @as(c_int, 2048);
pub const ACPI_MP_WAKE_COMMAND_WAKEUP = @as(c_int, 1);
pub const ACPI_MADT_ENABLED = @as(c_int, 1);
pub const ACPI_MADT_ONLINE_CAPABLE = @as(c_int, 2);
pub const ACPI_MADT_POLARITY_MASK = @as(c_int, 3);
pub const ACPI_MADT_TRIGGER_MASK = @as(c_int, 3) << @as(c_int, 2);
pub const ACPI_MADT_POLARITY_CONFORMS = @as(c_int, 0);
pub const ACPI_MADT_POLARITY_ACTIVE_HIGH = @as(c_int, 1);
pub const ACPI_MADT_POLARITY_RESERVED = @as(c_int, 2);
pub const ACPI_MADT_POLARITY_ACTIVE_LOW = @as(c_int, 3);
pub const ACPI_MADT_TRIGGER_CONFORMS = @as(c_int, 0);
pub const ACPI_MADT_TRIGGER_EDGE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_MADT_TRIGGER_RESERVED = @as(c_int, 2) << @as(c_int, 2);
pub const ACPI_MADT_TRIGGER_LEVEL = @as(c_int, 3) << @as(c_int, 2);
pub const ACPI_MPST_ENABLED = @as(c_int, 1);
pub const ACPI_MPST_POWER_MANAGED = @as(c_int, 2);
pub const ACPI_MPST_HOT_PLUG_CAPABLE = @as(c_int, 4);
pub const ACPI_MPST_PRESERVE = @as(c_int, 1);
pub const ACPI_MPST_AUTOENTRY = @as(c_int, 2);
pub const ACPI_MPST_AUTOEXIT = @as(c_int, 4);
pub const ACPI_NFIT_ADD_ONLINE_ONLY = @as(c_int, 1);
pub const ACPI_NFIT_PROXIMITY_VALID = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_NFIT_LOCATION_COOKIE_VALID = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_NFIT_MEM_SAVE_FAILED = @as(c_int, 1);
pub const ACPI_NFIT_MEM_RESTORE_FAILED = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_NFIT_MEM_FLUSH_FAILED = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_NFIT_MEM_NOT_ARMED = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_NFIT_MEM_HEALTH_OBSERVED = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_NFIT_MEM_HEALTH_ENABLED = @as(c_int, 1) << @as(c_int, 5);
pub const ACPI_NFIT_MEM_MAP_FAILED = @as(c_int, 1) << @as(c_int, 6);
pub const ACPI_NFIT_CONTROL_BUFFERED = @as(c_int, 1);
pub const ACPI_NFIT_CONTROL_MFG_INFO_VALID = @as(c_int, 1);
pub const ACPI_NFIT_CAPABILITY_CACHE_FLUSH = @as(c_int, 1);
pub const ACPI_NFIT_CAPABILITY_MEM_FLUSH = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_NFIT_CAPABILITY_MEM_MIRRORING = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_NFIT_DIMM_NUMBER_MASK = @as(c_int, 0x0000000F);
pub const ACPI_NFIT_CHANNEL_NUMBER_MASK = @as(c_int, 0x000000F0);
pub const ACPI_NFIT_MEMORY_ID_MASK = @as(c_int, 0x00000F00);
pub const ACPI_NFIT_SOCKET_ID_MASK = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x0000F000, .hexadecimal);
pub const ACPI_NFIT_NODE_ID_MASK = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x0FFF0000, .hexadecimal);
pub const ACPI_NFIT_DIMM_NUMBER_OFFSET = @as(c_int, 0);
pub const ACPI_NFIT_CHANNEL_NUMBER_OFFSET = @as(c_int, 4);
pub const ACPI_NFIT_MEMORY_ID_OFFSET = @as(c_int, 8);
pub const ACPI_NFIT_SOCKET_ID_OFFSET = @as(c_int, 12);
pub const ACPI_NFIT_NODE_ID_OFFSET = @as(c_int, 16);
pub inline fn ACPI_NFIT_BUILD_DEVICE_HANDLE(dimm: anytype, channel: anytype, memory: anytype, socket: anytype, node_1: anytype) @TypeOf((((dimm | (channel << ACPI_NFIT_CHANNEL_NUMBER_OFFSET)) | (memory << ACPI_NFIT_MEMORY_ID_OFFSET)) | (socket << ACPI_NFIT_SOCKET_ID_OFFSET)) | (node_1 << ACPI_NFIT_NODE_ID_OFFSET)) {
    return (((dimm | (channel << ACPI_NFIT_CHANNEL_NUMBER_OFFSET)) | (memory << ACPI_NFIT_MEMORY_ID_OFFSET)) | (socket << ACPI_NFIT_SOCKET_ID_OFFSET)) | (node_1 << ACPI_NFIT_NODE_ID_OFFSET);
}
pub inline fn ACPI_NFIT_GET_DIMM_NUMBER(handle: anytype) @TypeOf(handle & ACPI_NFIT_DIMM_NUMBER_MASK) {
    return handle & ACPI_NFIT_DIMM_NUMBER_MASK;
}
pub inline fn ACPI_NFIT_GET_CHANNEL_NUMBER(handle: anytype) @TypeOf((handle & ACPI_NFIT_CHANNEL_NUMBER_MASK) >> ACPI_NFIT_CHANNEL_NUMBER_OFFSET) {
    return (handle & ACPI_NFIT_CHANNEL_NUMBER_MASK) >> ACPI_NFIT_CHANNEL_NUMBER_OFFSET;
}
pub inline fn ACPI_NFIT_GET_MEMORY_ID(handle: anytype) @TypeOf((handle & ACPI_NFIT_MEMORY_ID_MASK) >> ACPI_NFIT_MEMORY_ID_OFFSET) {
    return (handle & ACPI_NFIT_MEMORY_ID_MASK) >> ACPI_NFIT_MEMORY_ID_OFFSET;
}
pub inline fn ACPI_NFIT_GET_SOCKET_ID(handle: anytype) @TypeOf((handle & ACPI_NFIT_SOCKET_ID_MASK) >> ACPI_NFIT_SOCKET_ID_OFFSET) {
    return (handle & ACPI_NFIT_SOCKET_ID_MASK) >> ACPI_NFIT_SOCKET_ID_OFFSET;
}
pub inline fn ACPI_NFIT_GET_NODE_ID(handle: anytype) @TypeOf((handle & ACPI_NFIT_NODE_ID_MASK) >> ACPI_NFIT_NODE_ID_OFFSET) {
    return (handle & ACPI_NFIT_NODE_ID_MASK) >> ACPI_NFIT_NODE_ID_OFFSET;
}
pub const ACPI_PCCT_DOORBELL = @as(c_int, 1);
pub const ACPI_PCCT_INTERRUPT_POLARITY = @as(c_int, 1);
pub const ACPI_PCCT_INTERRUPT_MODE = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_PDTT_RUNTIME_TRIGGER = @as(c_int, 1);
pub const ACPI_PDTT_WAIT_COMPLETION = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_PDTT_TRIGGER_ORDER = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_PHAT_TYPE_FW_VERSION_DATA = @as(c_int, 0);
pub const ACPI_PHAT_TYPE_FW_HEALTH_DATA = @as(c_int, 1);
pub const ACPI_PHAT_TYPE_RESERVED = @as(c_int, 2);
pub const ACPI_PHAT_ERRORS_FOUND = @as(c_int, 0);
pub const ACPI_PHAT_NO_ERRORS = @as(c_int, 1);
pub const ACPI_PHAT_UNKNOWN_ERRORS = @as(c_int, 2);
pub const ACPI_PHAT_ADVISORY = @as(c_int, 3);
pub const ACPI_PMTT_TYPE_SOCKET = @as(c_int, 0);
pub const ACPI_PMTT_TYPE_CONTROLLER = @as(c_int, 1);
pub const ACPI_PMTT_TYPE_DIMM = @as(c_int, 2);
pub const ACPI_PMTT_TYPE_RESERVED = @as(c_int, 3);
pub const ACPI_PMTT_TYPE_VENDOR = @as(c_int, 0xFF);
pub const ACPI_PMTT_TOP_LEVEL = @as(c_int, 0x0001);
pub const ACPI_PMTT_PHYSICAL = @as(c_int, 0x0002);
pub const ACPI_PMTT_MEMORY_TYPE = @as(c_int, 0x000C);
pub const ACPI_PPTT_PHYSICAL_PACKAGE = @as(c_int, 1);
pub const ACPI_PPTT_ACPI_PROCESSOR_ID_VALID = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_PPTT_ACPI_PROCESSOR_IS_THREAD = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_PPTT_ACPI_LEAF_NODE = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_PPTT_ACPI_IDENTICAL = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_PPTT_SIZE_PROPERTY_VALID = @as(c_int, 1);
pub const ACPI_PPTT_NUMBER_OF_SETS_VALID = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_PPTT_ASSOCIATIVITY_VALID = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_PPTT_ALLOCATION_TYPE_VALID = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_PPTT_CACHE_TYPE_VALID = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_PPTT_WRITE_POLICY_VALID = @as(c_int, 1) << @as(c_int, 5);
pub const ACPI_PPTT_LINE_SIZE_VALID = @as(c_int, 1) << @as(c_int, 6);
pub const ACPI_PPTT_CACHE_ID_VALID = @as(c_int, 1) << @as(c_int, 7);
pub const ACPI_PPTT_MASK_ALLOCATION_TYPE = @as(c_int, 0x03);
pub const ACPI_PPTT_MASK_CACHE_TYPE = @as(c_int, 0x0C);
pub const ACPI_PPTT_MASK_WRITE_POLICY = @as(c_int, 0x10);
pub const ACPI_PPTT_CACHE_READ_ALLOCATE = @as(c_int, 0x0);
pub const ACPI_PPTT_CACHE_WRITE_ALLOCATE = @as(c_int, 0x01);
pub const ACPI_PPTT_CACHE_RW_ALLOCATE = @as(c_int, 0x02);
pub const ACPI_PPTT_CACHE_RW_ALLOCATE_ALT = @as(c_int, 0x03);
pub const ACPI_PPTT_CACHE_TYPE_DATA = @as(c_int, 0x0);
pub const ACPI_PPTT_CACHE_TYPE_INSTR = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_PPTT_CACHE_TYPE_UNIFIED = @as(c_int, 2) << @as(c_int, 2);
pub const ACPI_PPTT_CACHE_TYPE_UNIFIED_ALT = @as(c_int, 3) << @as(c_int, 2);
pub const ACPI_PPTT_CACHE_POLICY_WB = @as(c_int, 0x0);
pub const ACPI_PPTT_CACHE_POLICY_WT = @as(c_int, 1) << @as(c_int, 4);
pub const ACPI_RASF_SCRUBBER_RUNNING = @as(c_int, 1);
pub const ACPI_RASF_SPEED = @as(c_int, 7) << @as(c_int, 1);
pub const ACPI_RASF_SPEED_SLOW = @as(c_int, 0) << @as(c_int, 1);
pub const ACPI_RASF_SPEED_MEDIUM = @as(c_int, 4) << @as(c_int, 1);
pub const ACPI_RASF_SPEED_FAST = @as(c_int, 7) << @as(c_int, 1);
pub const ACPI_RASF_GENERATE_SCI = @as(c_int, 1) << @as(c_int, 15);
pub const ACPI_RASF_COMMAND_COMPLETE = @as(c_int, 1);
pub const ACPI_RASF_SCI_DOORBELL = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_RASF_ERROR = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_RASF_STATUS = @as(c_int, 0x1F) << @as(c_int, 3);
pub const ACPI_RAS2_SCRUBBER_RUNNING = @as(c_int, 1);
pub const ACPI_RAS2_GENERATE_SCI = @as(c_int, 1) << @as(c_int, 15);
pub const ACPI_RAS2_COMMAND_COMPLETE = @as(c_int, 1);
pub const ACPI_RAS2_SCI_DOORBELL = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_RAS2_ERROR = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_RAS2_STATUS = @as(c_int, 0x1F) << @as(c_int, 3);
pub const ACPI_RHCT_TIMER_CANNOT_WAKEUP_CPU = @as(c_int, 1);
pub const ACPI_SDEV_HANDOFF_TO_UNSECURE_OS = @as(c_int, 1);
pub const ACPI_SDEV_SECURE_COMPONENTS_PRESENT = @as(c_int, 1) << @as(c_int, 1);
pub const __ACTBL3_H__ = "";
pub const ACPI_SIG_SLIC = "SLIC";
pub const ACPI_SIG_SLIT = "SLIT";
pub const ACPI_SIG_SPCR = "SPCR";
pub const ACPI_SIG_SPMI = "SPMI";
pub const ACPI_SIG_SRAT = "SRAT";
pub const ACPI_SIG_STAO = "STAO";
pub const ACPI_SIG_TCPA = "TCPA";
pub const ACPI_SIG_TPM2 = "TPM2";
pub const ACPI_SIG_UEFI = "UEFI";
pub const ACPI_SIG_VIOT = "VIOT";
pub const ACPI_SIG_WAET = "WAET";
pub const ACPI_SIG_WDAT = "WDAT";
pub const ACPI_SIG_WDDT = "WDDT";
pub const ACPI_SIG_WDRT = "WDRT";
pub const ACPI_SIG_WPBT = "WPBT";
pub const ACPI_SIG_WSMT = "WSMT";
pub const ACPI_SIG_XENV = "XENV";
pub const ACPI_SIG_XXXX = "XXXX";
pub const ACPI_SPCR_DO_NOT_DISABLE = @as(c_int, 1);
pub const ACPI_SRAT_CPU_USE_AFFINITY = @as(c_int, 1);
pub const ACPI_SRAT_MEM_ENABLED = @as(c_int, 1);
pub const ACPI_SRAT_MEM_HOT_PLUGGABLE = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_SRAT_MEM_NON_VOLATILE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_SRAT_CPU_ENABLED = @as(c_int, 1);
pub const ACPI_SRAT_GICC_ENABLED = @as(c_int, 1);
pub const ACPI_SRAT_DEVICE_HANDLE_SIZE = @as(c_int, 16);
pub const ACPI_SRAT_GENERIC_AFFINITY_ENABLED = @as(c_int, 1);
pub const ACPI_SRAT_ARCHITECTURAL_TRANSACTIONS = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_TCPA_CLIENT_TABLE = @as(c_int, 0);
pub const ACPI_TCPA_SERVER_TABLE = @as(c_int, 1);
pub const ACPI_TCPA_PCI_DEVICE = @as(c_int, 1);
pub const ACPI_TCPA_BUS_PNP = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_TCPA_ADDRESS_VALID = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_TCPA_INTERRUPT_MODE = @as(c_int, 1);
pub const ACPI_TCPA_INTERRUPT_POLARITY = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_TCPA_SCI_VIA_GPE = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_TCPA_GLOBAL_INTERRUPT = @as(c_int, 1) << @as(c_int, 3);
pub const ACPI_TPM23_ACPI_START_METHOD = @as(c_int, 2);
pub const ACPI_TPM2_NOT_ALLOWED = @as(c_int, 0);
pub const ACPI_TPM2_RESERVED1 = @as(c_int, 1);
pub const ACPI_TPM2_START_METHOD = @as(c_int, 2);
pub const ACPI_TPM2_RESERVED3 = @as(c_int, 3);
pub const ACPI_TPM2_RESERVED4 = @as(c_int, 4);
pub const ACPI_TPM2_RESERVED5 = @as(c_int, 5);
pub const ACPI_TPM2_MEMORY_MAPPED = @as(c_int, 6);
pub const ACPI_TPM2_COMMAND_BUFFER = @as(c_int, 7);
pub const ACPI_TPM2_COMMAND_BUFFER_WITH_START_METHOD = @as(c_int, 8);
pub const ACPI_TPM2_RESERVED9 = @as(c_int, 9);
pub const ACPI_TPM2_RESERVED10 = @as(c_int, 10);
pub const ACPI_TPM2_COMMAND_BUFFER_WITH_ARM_SMC = @as(c_int, 11);
pub const ACPI_TPM2_RESERVED = @as(c_int, 12);
pub const ACPI_TPM2_INTERRUPT_SUPPORT = @as(c_int, 1);
pub const ACPI_TPM2_IDLE_SUPPORT = @as(c_int, 1);
pub const ACPI_WAET_RTC_NO_ACK = @as(c_int, 1);
pub const ACPI_WAET_TIMER_ONE_READ = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_WDAT_ENABLED = @as(c_int, 1);
pub const ACPI_WDAT_STOPPED = @as(c_int, 0x80);
pub const ACPI_WDDT_AVAILABLE = @as(c_int, 1);
pub const ACPI_WDDT_ACTIVE = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_WDDT_TCO_OS_OWNED = @as(c_int, 1) << @as(c_int, 2);
pub const ACPI_WDDT_USER_RESET = @as(c_int, 1) << @as(c_int, 11);
pub const ACPI_WDDT_WDT_RESET = @as(c_int, 1) << @as(c_int, 12);
pub const ACPI_WDDT_POWER_FAIL = @as(c_int, 1) << @as(c_int, 13);
pub const ACPI_WDDT_UNKNOWN_RESET = @as(c_int, 1) << @as(c_int, 14);
pub const ACPI_WDDT_AUTO_RESET = @as(c_int, 1);
pub const ACPI_WDDT_ALERT_SUPPORT = @as(c_int, 1) << @as(c_int, 1);
pub const ACPI_WSMT_FIXED_COMM_BUFFERS = @as(c_int, 1);
pub const ACPI_WSMT_COMM_BUFFER_NESTED_PTR_PROTECTION = @as(c_int, 2);
pub const ACPI_WSMT_SYSTEM_RESOURCE_PROTECTION = @as(c_int, 4);
pub inline fn ACPI_FADT_OFFSET(f: anytype) UINT16 {
    return @import("std").zig.c_translation.cast(UINT16, ACPI_OFFSET(ACPI_TABLE_FADT, f));
}
pub const ACPI_FADT_V6_SIZE = @import("std").zig.c_translation.cast(UINT32, @import("std").zig.c_translation.sizeof(ACPI_TABLE_FADT));
pub const ACPI_FADT_CONFORMANCE = "ACPI 6.1 (FADT version 6)";
pub const DEV_FLAG_SOFTDEV = @as(c_int, 0x00000001);
pub const DEV_FLAG_CONTROLLER = @as(c_int, 0x00000002);
pub const DEV_FLAG_POWERED = @as(c_int, 0x00000004);
pub const DEV_FLAG_BUS = @as(c_int, 0x00000008);
pub const DEV_FLAG_ERROR = @as(c_int, 0x00000010);
pub const DEV_FLAG_CORE = @as(c_int, 0x00000020);
pub const __ANIVA_DRIVER__ = "";
pub const __ANIVA_VID_DEV_PRECEDENCE__ = "";
pub const DRV_PRECEDENCE_BASIC = @as(c_int, 10);
pub const DRV_PRECEDENCE_MID = @as(c_int, 50);
pub const DRV_PRECEDENCE_HIGH = @as(c_int, 100);
pub const DRV_PRECEDENCE_EXTREME = @as(c_int, 200);
pub const DRV_PRECEDENCE_FULL = @as(c_int, 0xff);
pub const DRVDEP_FLAG_RELPATH = @as(c_int, 0x00000001);
pub const DRVDEP_FLAG_OPTIONAL = @as(c_int, 0x00000002);
pub const DRV_FS = @as(c_int, 0x00000001);
pub const DRV_NON_ESSENTIAL = @as(c_int, 0x00000002);
pub const DRV_ACTIVE = @as(c_int, 0x00000004);
pub const DRV_ALLOW_DYNAMIC_LOADING = @as(c_int, 0x00000008);
pub const DRV_HAS_HANDLE = @as(c_int, 0x00000010);
pub const DRV_FAILED = @as(c_int, 0x00000020);
pub const DRV_DEFERRED = @as(c_int, 0x00000040);
pub const DRV_LIMIT_REACHED = @as(c_int, 0x00000080);
pub const DRV_WANT_PROC = @as(c_int, 0x00000100);
pub const DRV_HAD_DEP = @as(c_int, 0x00000200);
pub const DRV_SYSTEM = @as(c_int, 0x00000400);
pub const DRV_ORPHAN = @as(c_int, 0x00000800);
pub const DRV_IS_EXTERNAL = @as(c_int, 0x00001000);
pub const DRV_HAS_MSG_FUNC = @as(c_int, 0x00002000);
pub const DRV_DEFERRED_HNDL = @as(c_int, 0x00004000);
pub const DRV_CORE = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x00008000, .hexadecimal);
pub const DRV_LOADED = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x00010000, .hexadecimal);
pub const DRV_STAT_OK = @as(c_int, 0);
pub const DRV_STAT_INVAL = -@as(c_int, 1);
pub const DRV_STAT_NOMAN = -@as(c_int, 2);
pub const DRV_STAT_BUSY = -@as(c_int, 3);
pub inline fn driver_is_deferred(manifest: anytype) @TypeOf((manifest.*.m_flags & DRV_DEFERRED) == DRV_DEFERRED) {
    return (manifest.*.m_flags & DRV_DEFERRED) == DRV_DEFERRED;
}
pub const __ANIVA_DRV_MANIFEST__ = "";
pub const __ANIVA_SYS_RESOURCE__ = "";
pub const __ANIVA_LIBK_REFERENCE__ = "";
pub const __ANIVA_IO__ = "";
pub inline fn mdelay(msec: anytype) @TypeOf(udelay(msec * @as(c_int, 1000))) {
    return udelay(msec * @as(c_int, 1000));
}
pub const __ANIVA_HEAP_CORE__ = "";
pub const __ANIVA_MALLOC__ = "";
pub const __SERIAL__ = "";
pub const COM1 = @import("std").zig.c_translation.cast(u16, @as(c_int, 0x3F8));
pub const __ANIVA_MEM_PAGEDIR__ = "";
pub const MALLOC_NODE_IDENTIFIER = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xF0CEDA22, .hexadecimal);
pub const GHEAP_READONLY = @as(c_int, 1) << @as(c_int, 0);
pub const GHEAP_KERNEL = @as(c_int, 1) << @as(c_int, 1);
pub const GHEAP_EXPANDABLE = @as(c_int, 1) << @as(c_int, 2);
pub const GHEAP_ZEROED = @as(c_int, 1) << @as(c_int, 3);
pub const GHEAP_NOBLOCK = @as(c_int, 1) << @as(c_int, 4);
pub const KRES_TYPE_IO = @as(c_int, 0);
pub const KRES_TYPE_MEM = @as(c_int, 1);
pub const KRES_TYPE_IRQ = @as(c_int, 2);
pub const KRES_MIN_TYPE = KRES_TYPE_IO;
pub const KRES_MAX_TYPE = KRES_TYPE_IRQ;
pub const KRES_TYPE_COUNT = KRES_MAX_TYPE + @as(c_int, 1);
pub const KRES_FLAG_MEM_KEEP_PHYS = @as(c_int, 0x0001);
pub const KRES_FLAG_IO__RES0 = @as(c_int, 0x0001);
pub const KRES_FLAG_IRQ_RES0 = @as(c_int, 0x0001);
pub const KRES_FLAG_SHARED = @as(c_int, 0x0002);
pub const KRES_FLAG_NEVER_SHARE = @as(c_int, 0x0004);
pub const KRES_FLAG_PROTECTED_R = @as(c_int, 0x0008);
pub const KRES_FLAG_PROTECTED_W = @as(c_int, 0x0010);
pub const KRES_FLAG_PROTECTED_RW = KRES_FLAG_PROTECTED_R | KRES_FLAG_PROTECTED_W;
pub const KRES_FLAG_UNBACKED = @as(c_int, 0x0020);
pub const KRES_FLAG_ONDISK = @as(c_int, 0x0040);
pub inline fn GET_RESOURCE(bundle: anytype, @"type": anytype) @TypeOf(bundle.*.resources[@as(usize, @intCast(@"type"))]) {
    return bundle.*.resources[@as(usize, @intCast(@"type"))];
}
pub const __ANIVA_LIBK_HASHMAP__ = "";
pub const HASHMAP_FLAG_SK = @as(c_int, 0x00000001);
pub const HASHMAP_FLAG_CA = @as(c_int, 0x00000002);
pub const HASHMAP_FLAG_FS = @as(c_int, 0x00000004);
pub const __KMEM_MANAGER__ = "";
pub const MULTIBOOT_HEADER = @as(c_int, 1);
pub const MULTIBOOT_SEARCH = @import("std").zig.c_translation.promoteIntLiteral(c_int, 32768, .decimal);
pub const MULTIBOOT_HEADER_ALIGN = @as(c_int, 8);
pub const MULTIBOOT2_HEADER_MAGIC = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xe85250d6, .hexadecimal);
pub const MULTIBOOT2_BOOTLOADER_MAGIC = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x36d76289, .hexadecimal);
pub const MULTIBOOT_MOD_ALIGN = @as(c_int, 0x00001000);
pub const MULTIBOOT_INFO_ALIGN = @as(c_int, 0x00000008);
pub const MULTIBOOT_TAG_ALIGN = @as(c_int, 8);
pub const MULTIBOOT_TAG_TYPE_END = @as(c_int, 0);
pub const MULTIBOOT_TAG_TYPE_CMDLINE = @as(c_int, 1);
pub const MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME = @as(c_int, 2);
pub const MULTIBOOT_TAG_TYPE_MODULE = @as(c_int, 3);
pub const MULTIBOOT_TAG_TYPE_BASIC_MEMINFO = @as(c_int, 4);
pub const MULTIBOOT_TAG_TYPE_BOOTDEV = @as(c_int, 5);
pub const MULTIBOOT_TAG_TYPE_MMAP = @as(c_int, 6);
pub const MULTIBOOT_TAG_TYPE_VBE = @as(c_int, 7);
pub const MULTIBOOT_TAG_TYPE_FRAMEBUFFER = @as(c_int, 8);
pub const MULTIBOOT_TAG_TYPE_ELF_SECTIONS = @as(c_int, 9);
pub const MULTIBOOT_TAG_TYPE_APM = @as(c_int, 10);
pub const MULTIBOOT_TAG_TYPE_EFI32 = @as(c_int, 11);
pub const MULTIBOOT_TAG_TYPE_EFI64 = @as(c_int, 12);
pub const MULTIBOOT_TAG_TYPE_SMBIOS = @as(c_int, 13);
pub const MULTIBOOT_TAG_TYPE_ACPI_OLD = @as(c_int, 14);
pub const MULTIBOOT_TAG_TYPE_ACPI_NEW = @as(c_int, 15);
pub const MULTIBOOT_TAG_TYPE_NETWORK = @as(c_int, 16);
pub const MULTIBOOT_TAG_TYPE_EFI_MMAP = @as(c_int, 17);
pub const MULTIBOOT_TAG_TYPE_EFI_BS = @as(c_int, 18);
pub const MULTIBOOT_TAG_TYPE_EFI32_IH = @as(c_int, 19);
pub const MULTIBOOT_TAG_TYPE_EFI64_IH = @as(c_int, 20);
pub const MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR = @as(c_int, 21);
pub const MULTIBOOT_TAG_TYPE_KFLAGS = @as(c_int, 22);
pub const MULTIBOOT_HEADER_TAG_END = @as(c_int, 0);
pub const MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST = @as(c_int, 1);
pub const MULTIBOOT_HEADER_TAG_ADDRESS = @as(c_int, 2);
pub const MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS = @as(c_int, 3);
pub const MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS = @as(c_int, 4);
pub const MULTIBOOT_HEADER_TAG_FRAMEBUFFER = @as(c_int, 5);
pub const MULTIBOOT_HEADER_TAG_MODULE_ALIGN = @as(c_int, 6);
pub const MULTIBOOT_HEADER_TAG_EFI_BS = @as(c_int, 7);
pub const MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI32 = @as(c_int, 8);
pub const MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64 = @as(c_int, 9);
pub const MULTIBOOT_HEADER_TAG_RELOCATABLE = @as(c_int, 10);
pub const MULTIBOOT_ARCHITECTURE_I386 = @as(c_int, 0);
pub const MULTIBOOT_ARCHITECTURE_MIPS32 = @as(c_int, 4);
pub const MULTIBOOT_HEADER_TAG_OPTIONAL = @as(c_int, 1);
pub const MULTIBOOT_LOAD_PREFERENCE_NONE = @as(c_int, 0);
pub const MULTIBOOT_LOAD_PREFERENCE_LOW = @as(c_int, 1);
pub const MULTIBOOT_LOAD_PREFERENCE_HIGH = @as(c_int, 2);
pub const MULTIBOOT_CONSOLE_FLAGS_CONSOLE_REQUIRED = @as(c_int, 1);
pub const MULTIBOOT_CONSOLE_FLAGS_EGA_TEXT_SUPPORTED = @as(c_int, 2);
pub const MULTIBOOT_MEMORY_AVAILABLE = @as(c_int, 1);
pub const MULTIBOOT_MEMORY_RESERVED = @as(c_int, 2);
pub const MULTIBOOT_MEMORY_ACPI_RECLAIMABLE = @as(c_int, 3);
pub const MULTIBOOT_MEMORY_NVS = @as(c_int, 4);
pub const MULTIBOOT_MEMORY_BADRAM = @as(c_int, 5);
pub const MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED = @as(c_int, 0);
pub const MULTIBOOT_FRAMEBUFFER_TYPE_RGB = @as(c_int, 1);
pub const MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT = @as(c_int, 2);
pub const ANIVA_KFLAG_DEBUG = @as(c_ulonglong, 0x0000000000000001);
pub const ANIVA_KFLAG_LAUNCH_KTERM = @as(c_ulonglong, 0x0000000000000002);
pub const ANIVA_KFLAG_USE_RD = @as(c_ulonglong, 0x0000000000000004);
pub const PRESENT_VIOLATION = @as(c_int, 0x1);
pub const WRITE_VIOLATION = @as(c_int, 0x2);
pub const ACCESS_VIOLATION = @as(c_int, 0x4);
pub const RESERVED_VIOLATION = @as(c_int, 0x8);
pub const FETCH_VIOLATION = @as(c_int, 0x10);
pub const MAX_VIRT_ADDR = @as(c_ulonglong, 0xffffffffffffffff);
pub const HIGH_MAP_BASE = @as(c_ulonglong, 0xFFFFFFFF80000000);
pub const KERNEL_MAP_BASE = @as(c_ulonglong, 0xFFFF800000000000);
pub const IO_MAP_BASE = @as(c_ulonglong, 0xffff000000000000);
pub const EARLY_FB_MAP_BASE = @as(c_ulonglong, 0xffffff8000000000);
pub const QUICKMAP_BASE = @as(c_ulonglong, 0xFFFFffffFFFF0000);
pub const HIGH_STACK_BASE = @as(c_ulonglong, 0x0000000800000000);
pub const THREAD_ENTRY_BASE = @as(c_ulonglong, 0xFFFFFFFF00000000);
pub const PDE_SIZE_MASK = @as(c_ulonglong, 0xFFFFffffFFFFF000);
pub const PAGE_LOW_MASK = @as(c_ulonglong, 0xFFF);
pub const PML_ENTRY_MASK = @as(c_ulonglong, 0x8000000000000fff);
pub const ENTRY_MASK = @as(c_ulonglong, 0x1FF);
pub const PAGE_SIZE = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x200000, .hexadecimal);
pub const PAGE_SHIFT = @as(c_int, 12);
pub const LARGE_PAGE_SHIFT = @as(c_int, 21);
pub const SMALL_PAGE_SIZE = @as(c_int, 1) << PAGE_SHIFT;
pub const LARGE_PAGE_SIZE = @as(c_int, 1) << LARGE_PAGE_SHIFT;
pub const PAGE_SIZE_BYTES = @as(c_ulong, 0x200000);
pub const PDP_MASK = @as(c_ulong, 0x3fffffff);
pub const PD_MASK = @as(c_ulong, 0x1fffff);
pub const PT_MASK = PAGE_LOW_MASK;
pub const KMEM_FLAG_KERNEL = @as(c_int, 0x01);
pub const KMEM_FLAG_WRITABLE = @as(c_int, 0x02);
pub const KMEM_FLAG_NOCACHE = @as(c_int, 0x04);
pub const KMEM_FLAG_WRITETHROUGH = @as(c_int, 0x08);
pub const KMEM_FLAG_SPEC = @as(c_int, 0x10);
pub const KMEM_FLAG_WC = (KMEM_FLAG_NOCACHE | KMEM_FLAG_WRITETHROUGH) | KMEM_FLAG_SPEC;
pub const KMEM_FLAG_DMA = KMEM_FLAG_NOCACHE | KMEM_FLAG_WRITABLE;
pub const KMEM_FLAG_NOEXECUTE = @as(c_int, 0x20);
pub const KMEM_FLAG_GLOBAL = @as(c_int, 0x40);
pub const KMEM_CUSTOMFLAG_GET_MAKE = @as(c_int, 0x00000001);
pub const KMEM_CUSTOMFLAG_CREATE_USER = @as(c_int, 0x00000002);
pub const KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE = @as(c_int, 0x00000004);
pub const KMEM_CUSTOMFLAG_IDENTITY = @as(c_int, 0x00000008);
pub const KMEM_CUSTOMFLAG_NO_REMAP = @as(c_int, 0x00000010);
pub const KMEM_CUSTOMFLAG_GIVE_PHYS = @as(c_int, 0x00000020);
pub const KMEM_CUSTOMFLAG_USE_QUICKMAP = @as(c_int, 0x00000040);
pub const KMEM_CUSTOMFLAG_UNMAP = @as(c_int, 0x00000080);
pub const KMEM_CUSTOMFLAG_NO_PHYS_REALIGN = @as(c_int, 0x00000100);
pub const KMEM_CUSTOMFLAG_NO_MARK = @as(c_int, 0x00000200);
pub const KMEM_CUSTOMFLAG_READONLY = @as(c_int, 0x00000400);
pub const KMEM_CUSTOMFLAG_RECURSIVE_UNMAP = @as(c_int, 0x00000800);
pub const KMEM_CUSTOMFLAG_2MIB_MAP = @as(c_int, 0x00001000);
pub const KMEM_STATUS_FLAG_DONE_INIT = @as(c_int, 0x00000001);
pub const KMEM_STATUS_FLAG_HAS_QUICKMAP = @as(c_int, 0x00000002);
pub inline fn ALIGN_UP(addr: anytype, size: anytype) @TypeOf(if (@import("std").zig.c_translation.MacroArithmetic.rem(addr, size) == @as(c_int, 0)) addr else (addr + size) - @import("std").zig.c_translation.MacroArithmetic.rem(addr, size)) {
    return if (@import("std").zig.c_translation.MacroArithmetic.rem(addr, size) == @as(c_int, 0)) addr else (addr + size) - @import("std").zig.c_translation.MacroArithmetic.rem(addr, size);
}
pub inline fn ALIGN_DOWN(addr: anytype, size: anytype) @TypeOf(addr - @import("std").zig.c_translation.MacroArithmetic.rem(addr, size)) {
    return addr - @import("std").zig.c_translation.MacroArithmetic.rem(addr, size);
}
pub inline fn GET_PAGECOUNT_EX(start: anytype, len: anytype, page_shift: anytype) @TypeOf((ALIGN_UP(start + len, SMALL_PAGE_SIZE) - ALIGN_DOWN(start, SMALL_PAGE_SIZE)) >> page_shift) {
    return (ALIGN_UP(start + len, SMALL_PAGE_SIZE) - ALIGN_DOWN(start, SMALL_PAGE_SIZE)) >> page_shift;
}
pub inline fn GET_PAGECOUNT(start: anytype, len: anytype) @TypeOf(GET_PAGECOUNT_EX(start, len, PAGE_SHIFT)) {
    return GET_PAGECOUNT_EX(start, len, PAGE_SHIFT);
}
pub const __ANIVA_ZIG_ABI_DISK_BINDINGS__ = "";
pub const __ANIVA_SHARED_DISK_DEFS__ = "";
pub const ATA_COMMAND_IDENTIFY = @as(c_int, 0xEC);
pub const __ANIVA_GENERIC_DISK_DEV__ = "";
pub const __ANIVA_PARTITION_MBR__ = "";
pub const MBR_SIGNATURE = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0xAA55, .hexadecimal);
pub const MBR_PROTECTIVE = @as(c_int, 0xEE);
pub const MBR_TYPE_EFI_SYS_PART = @as(c_int, 0xEF);
pub const EBR_CHS_CONTAINER = @as(c_int, 0x05);
pub const EBR_LBA_CONTAINER = @as(c_int, 0x0F);
pub const PART_TYPE_NONE = @as(c_int, 0);
pub const PART_TYPE_GPT = @as(c_int, 1);
pub const PART_TYPE_MBR = @as(c_int, 2);
pub const GDISKDEV_FLAG_SCSI = @as(c_int, 0x00000001);
pub const GDISKDEV_FLAG_RAM = @as(c_int, 0x00000002);
pub const GDISKDEV_FLAG_RAM_COMPRESSED = @as(c_int, 0x00000004);
pub const GDISKDEV_FLAG_RO = @as(c_int, 0x00000008);
pub const GDISKDEV_FLAG_NO_PART = @as(c_int, 0x00000010);
pub const GDISKDEV_FLAG_LEGACY = @as(c_int, 0x00000020);
pub const GDISKDEV_FLAG_WAS_MOUNTED = @as(c_int, 0x00000040);
pub const PD_FLAG_ONLY_SYNC = @as(c_int, 0x00000001);
pub const DISK_DCC_GET_DEVICE = @as(c_int, 10);
pub const __ANIVA_GPT_PARTITION__ = "";
pub const GPT_SIG_0 = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x20494645, .hexadecimal);
pub const GPT_SIG_1 = @import("std").zig.c_translation.promoteIntLiteral(c_int, 0x54524150, .hexadecimal);
pub const __va_list_tag = struct___va_list_tag;
pub const kerror_frame = struct_kerror_frame;
pub const _ANIVA_STATUS = enum__ANIVA_STATUS;
pub const _ErrorOrPtr = struct__ErrorOrPtr;
pub const node = struct_node;
pub const list = struct_list;
pub const hive = struct_hive;
pub const hive_entry = struct_hive_entry;
pub const HIVE_ENTRY_TYPE = enum_HIVE_ENTRY_TYPE;
pub const packet_response = struct_packet_response;
pub const oss_obj = struct_oss_obj;
pub const oss_node = struct_oss_node;
pub const driver_version = union_driver_version;
pub const vector = struct_vector;
pub const kresource = struct_kresource;
pub const kresource_bundle = struct_kresource_bundle;
pub const thread_state = enum_thread_state;
pub const proc = struct_proc;
pub const tspckt = struct_tspckt;
pub const packet_payload = struct_packet_payload;
pub const processor = struct_processor;
pub const spinlock = struct_spinlock;
pub const queue_entry = struct_queue_entry;
pub const queue = struct_queue;
pub const socket_packet_queue = struct_socket_packet_queue;
pub const THREADED_SOCKET_STATE = enum_THREADED_SOCKET_STATE;
pub const threaded_socket = struct_threaded_socket;
pub const thread = struct_thread;
pub const mutex = struct_mutex;
pub const extern_driver = struct_extern_driver;
pub const driver_ops = struct_driver_ops;
pub const drv_manifest = struct_drv_manifest;
pub const DGROUP_TYPE = enum_DGROUP_TYPE;
pub const dgroup = struct_dgroup;
pub const acpi_power_state = struct_acpi_power_state;
pub const acpi_device_power = struct_acpi_device_power;
pub const apci_device = struct_apci_device;
pub const ENDPOINT_TYPE = enum_ENDPOINT_TYPE;
pub const device_generic_endpoint = struct_device_generic_endpoint;
pub const device_disk_endpoint = struct_device_disk_endpoint;
pub const device_video_endpoint = struct_device_video_endpoint;
pub const device_hid_endpoint = struct_device_hid_endpoint;
pub const device_pwm_endpoint = struct_device_pwm_endpoint;
pub const device_endpoint = struct_device_endpoint;
pub const device = struct_device;
pub const DRV_DEPTYPE = enum_DRV_DEPTYPE;
pub const drv_dependency = struct_drv_dependency;
pub const aniva_driver = struct_aniva_driver;
pub const dev_flags = enum_dev_flags;
pub const dev_constraint = struct_dev_constraint;
pub const pml_entry = union_pml_entry;
pub const tss_entry = struct_tss_entry;
pub const registers = struct_registers;
pub const THREADED_SOCKET_FLAGS = enum_THREADED_SOCKET_FLAGS;
pub const state_comp = enum_state_comp;
pub const bitmap = struct_bitmap;
pub const BITMAP_SEARCH_DIR = enum_BITMAP_SEARCH_DIR;
pub const ZONE_FLAGS = enum_ZONE_FLAGS;
pub const ZONE_ENTRY_SIZE = enum_ZONE_ENTRY_SIZE;
pub const zone = struct_zone;
pub const zone_store = struct_zone_store;
pub const zone_allocator = struct_zone_allocator;
pub const acpi_sleep_functions = struct_acpi_sleep_functions;
pub const acpi_object = union_acpi_object;
pub const acpi_object_list = struct_acpi_object_list;
pub const acpi_buffer = struct_acpi_buffer;
pub const acpi_predefined_names = struct_acpi_predefined_names;
pub const acpi_system_info = struct_acpi_system_info;
pub const acpi_statistics = struct_acpi_statistics;
pub const acpi_connection_info = struct_acpi_connection_info;
pub const acpi_pcc_info = struct_acpi_pcc_info;
pub const acpi_ffh_info = struct_acpi_ffh_info;
pub const acpi_pnp_device_id = struct_acpi_pnp_device_id;
pub const acpi_pnp_device_id_list = struct_acpi_pnp_device_id_list;
pub const acpi_device_info = struct_acpi_device_info;
pub const acpi_pci_id = struct_acpi_pci_id;
pub const acpi_mem_mapping = struct_acpi_mem_mapping;
pub const acpi_mem_space_context = struct_acpi_mem_space_context;
pub const acpi_data_table_mapping = struct_acpi_data_table_mapping;
pub const acpi_memory_list = struct_acpi_memory_list;
pub const acpi_table_header = struct_acpi_table_header;
pub const acpi_generic_address = struct_acpi_generic_address;
pub const acpi_table_rsdp = struct_acpi_table_rsdp;
pub const acpi_rsdp_common = struct_acpi_rsdp_common;
pub const acpi_rsdp_extension = struct_acpi_rsdp_extension;
pub const acpi_table_rsdt = struct_acpi_table_rsdt;
pub const acpi_table_xsdt = struct_acpi_table_xsdt;
pub const acpi_table_facs = struct_acpi_table_facs;
pub const acpi_table_fadt = struct_acpi_table_fadt;
pub const AcpiPreferredPmProfiles = enum_AcpiPreferredPmProfiles;
pub const acpi_name_union = union_acpi_name_union;
pub const acpi_table_desc = struct_acpi_table_desc;
pub const acpi_subtable_header = struct_acpi_subtable_header;
pub const acpi_whea_header = struct_acpi_whea_header;
pub const acpi_table_asf = struct_acpi_table_asf;
pub const acpi_asf_header = struct_acpi_asf_header;
pub const AcpiAsfType = enum_AcpiAsfType;
pub const acpi_asf_info = struct_acpi_asf_info;
pub const acpi_asf_alert = struct_acpi_asf_alert;
pub const acpi_asf_alert_data = struct_acpi_asf_alert_data;
pub const acpi_asf_remote = struct_acpi_asf_remote;
pub const acpi_asf_control_data = struct_acpi_asf_control_data;
pub const acpi_asf_rmcp = struct_acpi_asf_rmcp;
pub const acpi_asf_address = struct_acpi_asf_address;
pub const acpi_table_aspt = struct_acpi_table_aspt;
pub const acpi_aspt_header = struct_acpi_aspt_header;
pub const AcpiAsptType = enum_AcpiAsptType;
pub const acpi_aspt_global_regs = struct_acpi_aspt_global_regs;
pub const acpi_aspt_sev_mbox_regs = struct_acpi_aspt_sev_mbox_regs;
pub const acpi_aspt_acpi_mbox_regs = struct_acpi_aspt_acpi_mbox_regs;
pub const acpi_table_bert = struct_acpi_table_bert;
pub const acpi_bert_region = struct_acpi_bert_region;
pub const AcpiBertErrorSeverity = enum_AcpiBertErrorSeverity;
pub const acpi_table_bgrt = struct_acpi_table_bgrt;
pub const acpi_table_boot = struct_acpi_table_boot;
pub const acpi_table_cdat = struct_acpi_table_cdat;
pub const acpi_cdat_header = struct_acpi_cdat_header;
pub const AcpiCdatType = enum_AcpiCdatType;
pub const acpi_cdat_dsmas = struct_acpi_cdat_dsmas;
pub const acpi_cdat_dslbis = struct_acpi_cdat_dslbis;
pub const acpi_cdat_dsmscis = struct_acpi_cdat_dsmscis;
pub const acpi_cdat_dsis = struct_acpi_cdat_dsis;
pub const acpi_cdat_dsemts = struct_acpi_cdat_dsemts;
pub const acpi_cdat_sslbis = struct_acpi_cdat_sslbis;
pub const acpi_cdat_sslbe = struct_acpi_cdat_sslbe;
pub const acpi_table_cedt = struct_acpi_table_cedt;
pub const acpi_cedt_header = struct_acpi_cedt_header;
pub const AcpiCedtType = enum_AcpiCedtType;
pub const acpi_cedt_chbs = struct_acpi_cedt_chbs;
pub const acpi_cedt_cfmws = struct_acpi_cedt_cfmws;
pub const acpi_cedt_cfmws_target_element = struct_acpi_cedt_cfmws_target_element;
pub const acpi_cedt_cxims = struct_acpi_cedt_cxims;
pub const acpi_cedt_rdpas = struct_acpi_cedt_rdpas;
pub const acpi_table_cpep = struct_acpi_table_cpep;
pub const acpi_cpep_polling = struct_acpi_cpep_polling;
pub const acpi_table_csrt = struct_acpi_table_csrt;
pub const acpi_csrt_group = struct_acpi_csrt_group;
pub const acpi_csrt_shared_info = struct_acpi_csrt_shared_info;
pub const acpi_csrt_descriptor = struct_acpi_csrt_descriptor;
pub const acpi_table_dbg2 = struct_acpi_table_dbg2;
pub const acpi_dbg2_header = struct_acpi_dbg2_header;
pub const acpi_dbg2_device = struct_acpi_dbg2_device;
pub const acpi_table_dbgp = struct_acpi_table_dbgp;
pub const acpi_table_dmar = struct_acpi_table_dmar;
pub const acpi_dmar_header = struct_acpi_dmar_header;
pub const AcpiDmarType = enum_AcpiDmarType;
pub const acpi_dmar_device_scope = struct_acpi_dmar_device_scope;
pub const AcpiDmarScopeType = enum_AcpiDmarScopeType;
pub const acpi_dmar_pci_path = struct_acpi_dmar_pci_path;
pub const acpi_dmar_hardware_unit = struct_acpi_dmar_hardware_unit;
pub const acpi_dmar_reserved_memory = struct_acpi_dmar_reserved_memory;
pub const acpi_dmar_atsr = struct_acpi_dmar_atsr;
pub const acpi_dmar_rhsa = struct_acpi_dmar_rhsa;
pub const acpi_dmar_andd = struct_acpi_dmar_andd;
pub const acpi_dmar_satc = struct_acpi_dmar_satc;
pub const acpi_table_drtm = struct_acpi_table_drtm;
pub const acpi_drtm_vtable_list = struct_acpi_drtm_vtable_list;
pub const acpi_drtm_resource = struct_acpi_drtm_resource;
pub const acpi_drtm_resource_list = struct_acpi_drtm_resource_list;
pub const acpi_drtm_dps_id = struct_acpi_drtm_dps_id;
pub const acpi_table_ecdt = struct_acpi_table_ecdt;
pub const acpi_table_einj = struct_acpi_table_einj;
pub const acpi_einj_entry = struct_acpi_einj_entry;
pub const AcpiEinjActions = enum_AcpiEinjActions;
pub const AcpiEinjInstructions = enum_AcpiEinjInstructions;
pub const acpi_einj_error_type_with_addr = struct_acpi_einj_error_type_with_addr;
pub const acpi_einj_vendor = struct_acpi_einj_vendor;
pub const acpi_einj_trigger = struct_acpi_einj_trigger;
pub const AcpiEinjCommandStatus = enum_AcpiEinjCommandStatus;
pub const acpi_table_erst = struct_acpi_table_erst;
pub const acpi_erst_entry = struct_acpi_erst_entry;
pub const AcpiErstActions = enum_AcpiErstActions;
pub const AcpiErstInstructions = enum_AcpiErstInstructions;
pub const AcpiErstCommandStatus = enum_AcpiErstCommandStatus;
pub const acpi_erst_info = struct_acpi_erst_info;
pub const acpi_table_fpdt = struct_acpi_table_fpdt;
pub const acpi_fpdt_header = struct_acpi_fpdt_header;
pub const AcpiFpdtType = enum_AcpiFpdtType;
pub const acpi_fpdt_boot_pointer = struct_acpi_fpdt_boot_pointer;
pub const acpi_fpdt_s3pt_pointer = struct_acpi_fpdt_s3pt_pointer;
pub const acpi_table_s3pt = struct_acpi_table_s3pt;
pub const AcpiS3ptType = enum_AcpiS3ptType;
pub const acpi_s3pt_resume = struct_acpi_s3pt_resume;
pub const acpi_s3pt_suspend = struct_acpi_s3pt_suspend;
pub const acpi_fpdt_boot = struct_acpi_fpdt_boot;
pub const acpi_table_gtdt = struct_acpi_table_gtdt;
pub const acpi_gtdt_el2 = struct_acpi_gtdt_el2;
pub const acpi_gtdt_header = struct_acpi_gtdt_header;
pub const AcpiGtdtType = enum_AcpiGtdtType;
pub const acpi_gtdt_timer_block = struct_acpi_gtdt_timer_block;
pub const acpi_gtdt_timer_entry = struct_acpi_gtdt_timer_entry;
pub const acpi_gtdt_watchdog = struct_acpi_gtdt_watchdog;
pub const acpi_table_hest = struct_acpi_table_hest;
pub const acpi_hest_header = struct_acpi_hest_header;
pub const AcpiHestTypes = enum_AcpiHestTypes;
pub const acpi_hest_ia_error_bank = struct_acpi_hest_ia_error_bank;
pub const acpi_hest_aer_common = struct_acpi_hest_aer_common;
pub const acpi_hest_notify = struct_acpi_hest_notify;
pub const AcpiHestNotifyTypes = enum_AcpiHestNotifyTypes;
pub const acpi_hest_ia_machine_check = struct_acpi_hest_ia_machine_check;
pub const acpi_hest_ia_corrected = struct_acpi_hest_ia_corrected;
pub const acpi_hest_ia_nmi = struct_acpi_hest_ia_nmi;
pub const acpi_hest_aer_root = struct_acpi_hest_aer_root;
pub const acpi_hest_aer = struct_acpi_hest_aer;
pub const acpi_hest_aer_bridge = struct_acpi_hest_aer_bridge;
pub const acpi_hest_generic = struct_acpi_hest_generic;
pub const acpi_hest_generic_v2 = struct_acpi_hest_generic_v2;
pub const acpi_hest_generic_status = struct_acpi_hest_generic_status;
pub const acpi_hest_generic_data = struct_acpi_hest_generic_data;
pub const acpi_hest_generic_data_v300 = struct_acpi_hest_generic_data_v300;
pub const acpi_hest_ia_deferred_check = struct_acpi_hest_ia_deferred_check;
pub const acpi_table_hmat = struct_acpi_table_hmat;
pub const AcpiHmatType = enum_AcpiHmatType;
pub const acpi_hmat_structure = struct_acpi_hmat_structure;
pub const acpi_hmat_proximity_domain = struct_acpi_hmat_proximity_domain;
pub const acpi_hmat_locality = struct_acpi_hmat_locality;
pub const acpi_hmat_cache = struct_acpi_hmat_cache;
pub const acpi_table_hpet = struct_acpi_table_hpet;
pub const AcpiHpetPageProtect = enum_AcpiHpetPageProtect;
pub const acpi_table_ibft = struct_acpi_table_ibft;
pub const acpi_ibft_header = struct_acpi_ibft_header;
pub const AcpiIbftType = enum_AcpiIbftType;
pub const acpi_ibft_control = struct_acpi_ibft_control;
pub const acpi_ibft_initiator = struct_acpi_ibft_initiator;
pub const acpi_ibft_nic = struct_acpi_ibft_nic;
pub const acpi_ibft_target = struct_acpi_ibft_target;
pub const acpi_table_aest = struct_acpi_table_aest;
pub const acpi_aest_hdr = struct_acpi_aest_hdr;
pub const acpi_aest_processor = struct_acpi_aest_processor;
pub const acpi_aest_processor_cache = struct_acpi_aest_processor_cache;
pub const acpi_aest_processor_tlb = struct_acpi_aest_processor_tlb;
pub const acpi_aest_processor_generic = struct_acpi_aest_processor_generic;
pub const acpi_aest_memory = struct_acpi_aest_memory;
pub const acpi_aest_smmu = struct_acpi_aest_smmu;
pub const acpi_aest_vendor = struct_acpi_aest_vendor;
pub const acpi_aest_gic = struct_acpi_aest_gic;
pub const acpi_aest_node_interface = struct_acpi_aest_node_interface;
pub const acpi_aest_node_interrupt = struct_acpi_aest_node_interrupt;
pub const acpi_table_agdi = struct_acpi_table_agdi;
pub const acpi_table_apmt = struct_acpi_table_apmt;
pub const acpi_apmt_node = struct_acpi_apmt_node;
pub const acpi_apmt_node_type = enum_acpi_apmt_node_type;
pub const acpi_table_bdat = struct_acpi_table_bdat;
pub const acpi_table_ccel = struct_acpi_table_ccel;
pub const acpi_table_iort = struct_acpi_table_iort;
pub const acpi_iort_node = struct_acpi_iort_node;
pub const AcpiIortNodeType = enum_AcpiIortNodeType;
pub const acpi_iort_id_mapping = struct_acpi_iort_id_mapping;
pub const acpi_iort_memory_access = struct_acpi_iort_memory_access;
pub const acpi_iort_its_group = struct_acpi_iort_its_group;
pub const acpi_iort_named_component = struct_acpi_iort_named_component;
pub const acpi_iort_root_complex = struct_acpi_iort_root_complex;
pub const acpi_iort_smmu = struct_acpi_iort_smmu;
pub const acpi_iort_smmu_gsi = struct_acpi_iort_smmu_gsi;
pub const acpi_iort_smmu_v3 = struct_acpi_iort_smmu_v3;
pub const acpi_iort_pmcg = struct_acpi_iort_pmcg;
pub const acpi_iort_rmr = struct_acpi_iort_rmr;
pub const acpi_iort_rmr_desc = struct_acpi_iort_rmr_desc;
pub const acpi_table_ivrs = struct_acpi_table_ivrs;
pub const acpi_ivrs_header = struct_acpi_ivrs_header;
pub const AcpiIvrsType = enum_AcpiIvrsType;
pub const acpi_ivrs_hardware_10 = struct_acpi_ivrs_hardware_10;
pub const acpi_ivrs_hardware_11 = struct_acpi_ivrs_hardware_11;
pub const acpi_ivrs_de_header = struct_acpi_ivrs_de_header;
pub const AcpiIvrsDeviceEntryType = enum_AcpiIvrsDeviceEntryType;
pub const acpi_ivrs_device4 = struct_acpi_ivrs_device4;
pub const acpi_ivrs_device8a = struct_acpi_ivrs_device8a;
pub const acpi_ivrs_device8b = struct_acpi_ivrs_device8b;
pub const acpi_ivrs_device8c = struct_acpi_ivrs_device8c;
pub const acpi_ivrs_device_hid = struct_acpi_ivrs_device_hid;
pub const acpi_ivrs_memory = struct_acpi_ivrs_memory;
pub const acpi_table_lpit = struct_acpi_table_lpit;
pub const acpi_lpit_header = struct_acpi_lpit_header;
pub const AcpiLpitType = enum_AcpiLpitType;
pub const acpi_lpit_native = struct_acpi_lpit_native;
pub const acpi_table_madt = struct_acpi_table_madt;
pub const AcpiMadtType = enum_AcpiMadtType;
pub const acpi_madt_local_apic = struct_acpi_madt_local_apic;
pub const acpi_madt_io_apic = struct_acpi_madt_io_apic;
pub const acpi_madt_interrupt_override = struct_acpi_madt_interrupt_override;
pub const acpi_madt_nmi_source = struct_acpi_madt_nmi_source;
pub const acpi_madt_local_apic_nmi = struct_acpi_madt_local_apic_nmi;
pub const acpi_madt_local_apic_override = struct_acpi_madt_local_apic_override;
pub const acpi_madt_io_sapic = struct_acpi_madt_io_sapic;
pub const acpi_madt_local_sapic = struct_acpi_madt_local_sapic;
pub const acpi_madt_interrupt_source = struct_acpi_madt_interrupt_source;
pub const acpi_madt_local_x2apic = struct_acpi_madt_local_x2apic;
pub const acpi_madt_local_x2apic_nmi = struct_acpi_madt_local_x2apic_nmi;
pub const acpi_madt_generic_interrupt = struct_acpi_madt_generic_interrupt;
pub const acpi_madt_generic_distributor = struct_acpi_madt_generic_distributor;
pub const AcpiMadtGicVersion = enum_AcpiMadtGicVersion;
pub const acpi_madt_generic_msi_frame = struct_acpi_madt_generic_msi_frame;
pub const acpi_madt_generic_redistributor = struct_acpi_madt_generic_redistributor;
pub const acpi_madt_generic_translator = struct_acpi_madt_generic_translator;
pub const acpi_madt_multiproc_wakeup = struct_acpi_madt_multiproc_wakeup;
pub const acpi_madt_multiproc_wakeup_mailbox = struct_acpi_madt_multiproc_wakeup_mailbox;
pub const acpi_madt_core_pic = struct_acpi_madt_core_pic;
pub const AcpiMadtCorePicVersion = enum_AcpiMadtCorePicVersion;
pub const acpi_madt_lio_pic = struct_acpi_madt_lio_pic;
pub const AcpiMadtLioPicVersion = enum_AcpiMadtLioPicVersion;
pub const acpi_madt_ht_pic = struct_acpi_madt_ht_pic;
pub const AcpiMadtHtPicVersion = enum_AcpiMadtHtPicVersion;
pub const acpi_madt_eio_pic = struct_acpi_madt_eio_pic;
pub const AcpiMadtEioPicVersion = enum_AcpiMadtEioPicVersion;
pub const acpi_madt_msi_pic = struct_acpi_madt_msi_pic;
pub const AcpiMadtMsiPicVersion = enum_AcpiMadtMsiPicVersion;
pub const acpi_madt_bio_pic = struct_acpi_madt_bio_pic;
pub const AcpiMadtBioPicVersion = enum_AcpiMadtBioPicVersion;
pub const acpi_madt_lpc_pic = struct_acpi_madt_lpc_pic;
pub const AcpiMadtLpcPicVersion = enum_AcpiMadtLpcPicVersion;
pub const acpi_madt_rintc = struct_acpi_madt_rintc;
pub const AcpiMadtRintcVersion = enum_AcpiMadtRintcVersion;
pub const acpi_madt_imsic = struct_acpi_madt_imsic;
pub const acpi_madt_aplic = struct_acpi_madt_aplic;
pub const acpi_madt_plic = struct_acpi_madt_plic;
pub const acpi_madt_oem_data = struct_acpi_madt_oem_data;
pub const acpi_mcfg_allocation = struct_acpi_mcfg_allocation;
pub const acpi_table_mcfg = struct_acpi_table_mcfg;
pub const acpi_table_mchi = struct_acpi_table_mchi;
pub const AcpiMpamLocatorType = enum_AcpiMpamLocatorType;
pub const acpi_mpam_func_deps = struct_acpi_mpam_func_deps;
pub const acpi_mpam_resource_cache_locator = struct_acpi_mpam_resource_cache_locator;
pub const acpi_mpam_resource_memory_locator = struct_acpi_mpam_resource_memory_locator;
pub const acpi_mpam_resource_smmu_locator = struct_acpi_mpam_resource_smmu_locator;
pub const acpi_mpam_resource_memcache_locator = struct_acpi_mpam_resource_memcache_locator;
pub const acpi_mpam_resource_acpi_locator = struct_acpi_mpam_resource_acpi_locator;
pub const acpi_mpam_resource_interconnect_locator = struct_acpi_mpam_resource_interconnect_locator;
pub const acpi_mpam_resource_generic_locator = struct_acpi_mpam_resource_generic_locator;
pub const acpi_mpam_resource_locator = union_acpi_mpam_resource_locator;
pub const acpi_mpam_resource_node = struct_acpi_mpam_resource_node;
pub const acpi_mpam_msc_node = struct_acpi_mpam_msc_node;
pub const acpi_table_mpam = struct_acpi_table_mpam;
pub const acpi_table_mpst = struct_acpi_table_mpst;
pub const acpi_mpst_channel = struct_acpi_mpst_channel;
pub const acpi_mpst_power_node = struct_acpi_mpst_power_node;
pub const acpi_mpst_power_state = struct_acpi_mpst_power_state;
pub const acpi_mpst_component = struct_acpi_mpst_component;
pub const acpi_mpst_data_hdr = struct_acpi_mpst_data_hdr;
pub const acpi_mpst_power_data = struct_acpi_mpst_power_data;
pub const acpi_mpst_shared = struct_acpi_mpst_shared;
pub const acpi_table_msct = struct_acpi_table_msct;
pub const acpi_msct_proximity = struct_acpi_msct_proximity;
pub const acpi_table_msdm = struct_acpi_table_msdm;
pub const acpi_table_nfit = struct_acpi_table_nfit;
pub const acpi_nfit_header = struct_acpi_nfit_header;
pub const AcpiNfitType = enum_AcpiNfitType;
pub const acpi_nfit_system_address = struct_acpi_nfit_system_address;
pub const acpi_nfit_memory_map = struct_acpi_nfit_memory_map;
pub const acpi_nfit_interleave = struct_acpi_nfit_interleave;
pub const acpi_nfit_smbios = struct_acpi_nfit_smbios;
pub const acpi_nfit_control_region = struct_acpi_nfit_control_region;
pub const acpi_nfit_data_region = struct_acpi_nfit_data_region;
pub const acpi_nfit_flush_address = struct_acpi_nfit_flush_address;
pub const acpi_nfit_capabilities = struct_acpi_nfit_capabilities;
pub const nfit_device_handle = struct_nfit_device_handle;
pub const acpi_table_pcct = struct_acpi_table_pcct;
pub const AcpiPcctType = enum_AcpiPcctType;
pub const acpi_pcct_subspace = struct_acpi_pcct_subspace;
pub const acpi_pcct_hw_reduced = struct_acpi_pcct_hw_reduced;
pub const acpi_pcct_hw_reduced_type2 = struct_acpi_pcct_hw_reduced_type2;
pub const acpi_pcct_ext_pcc_master = struct_acpi_pcct_ext_pcc_master;
pub const acpi_pcct_ext_pcc_slave = struct_acpi_pcct_ext_pcc_slave;
pub const acpi_pcct_hw_reg = struct_acpi_pcct_hw_reg;
pub const acpi_pcct_shared_memory = struct_acpi_pcct_shared_memory;
pub const acpi_pcct_ext_pcc_shared_memory = struct_acpi_pcct_ext_pcc_shared_memory;
pub const acpi_table_pdtt = struct_acpi_table_pdtt;
pub const acpi_pdtt_channel = struct_acpi_pdtt_channel;
pub const acpi_table_phat = struct_acpi_table_phat;
pub const acpi_phat_header = struct_acpi_phat_header;
pub const acpi_phat_version_data = struct_acpi_phat_version_data;
pub const acpi_phat_version_element = struct_acpi_phat_version_element;
pub const acpi_phat_health_data = struct_acpi_phat_health_data;
pub const acpi_table_pmtt = struct_acpi_table_pmtt;
pub const acpi_pmtt_header = struct_acpi_pmtt_header;
pub const acpi_pmtt_socket = struct_acpi_pmtt_socket;
pub const acpi_pmtt_controller = struct_acpi_pmtt_controller;
pub const acpi_pmtt_physical_component = struct_acpi_pmtt_physical_component;
pub const acpi_pmtt_vendor_specific = struct_acpi_pmtt_vendor_specific;
pub const acpi_table_pptt = struct_acpi_table_pptt;
pub const AcpiPpttType = enum_AcpiPpttType;
pub const acpi_pptt_processor = struct_acpi_pptt_processor;
pub const acpi_pptt_cache = struct_acpi_pptt_cache;
pub const acpi_pptt_cache_v1 = struct_acpi_pptt_cache_v1;
pub const acpi_pptt_id = struct_acpi_pptt_id;
pub const acpi_table_prmt = struct_acpi_table_prmt;
pub const acpi_table_prmt_header = struct_acpi_table_prmt_header;
pub const acpi_prmt_module_header = struct_acpi_prmt_module_header;
pub const acpi_prmt_module_info = struct_acpi_prmt_module_info;
pub const acpi_prmt_handler_info = struct_acpi_prmt_handler_info;
pub const acpi_table_rasf = struct_acpi_table_rasf;
pub const acpi_rasf_shared_memory = struct_acpi_rasf_shared_memory;
pub const acpi_rasf_parameter_block = struct_acpi_rasf_parameter_block;
pub const acpi_rasf_patrol_scrub_parameter = struct_acpi_rasf_patrol_scrub_parameter;
pub const AcpiRasfCommands = enum_AcpiRasfCommands;
pub const AcpiRasfCapabiliities = enum_AcpiRasfCapabiliities;
pub const AcpiRasfPatrolScrubCommands = enum_AcpiRasfPatrolScrubCommands;
pub const AcpiRasfStatus = enum_AcpiRasfStatus;
pub const acpi_table_ras2 = struct_acpi_table_ras2;
pub const acpi_ras2_pcc_desc = struct_acpi_ras2_pcc_desc;
pub const acpi_ras2_shared_memory = struct_acpi_ras2_shared_memory;
pub const acpi_ras2_parameter_block = struct_acpi_ras2_parameter_block;
pub const acpi_ras2_patrol_scrub_parameter = struct_acpi_ras2_patrol_scrub_parameter;
pub const acpi_ras2_la2pa_translation_parameter = struct_acpi_ras2_la2pa_translation_parameter;
pub const AcpiRas2Commands = enum_AcpiRas2Commands;
pub const AcpiRas2Features = enum_AcpiRas2Features;
pub const AcpiRas2PatrolScrubCommands = enum_AcpiRas2PatrolScrubCommands;
pub const AcpiRas2La2PaTranslationCommands = enum_AcpiRas2La2PaTranslationCommands;
pub const AcpiRas2La2PaTranslationStatus = enum_AcpiRas2La2PaTranslationStatus;
pub const AcpiRas2Status = enum_AcpiRas2Status;
pub const acpi_table_rgrt = struct_acpi_table_rgrt;
pub const AcpiRgrtImageType = enum_AcpiRgrtImageType;
pub const acpi_table_rhct = struct_acpi_table_rhct;
pub const acpi_rhct_node_header = struct_acpi_rhct_node_header;
pub const acpi_rhct_node_type = enum_acpi_rhct_node_type;
pub const acpi_rhct_isa_string = struct_acpi_rhct_isa_string;
pub const acpi_rhct_cmo_node = struct_acpi_rhct_cmo_node;
pub const acpi_rhct_mmu_node = struct_acpi_rhct_mmu_node;
pub const acpi_rhct_mmu_type = enum_acpi_rhct_mmu_type;
pub const acpi_rhct_hart_info = struct_acpi_rhct_hart_info;
pub const acpi_table_sbst = struct_acpi_table_sbst;
pub const acpi_table_sdei = struct_acpi_table_sdei;
pub const acpi_table_sdev = struct_acpi_table_sdev;
pub const acpi_sdev_header = struct_acpi_sdev_header;
pub const AcpiSdevType = enum_AcpiSdevType;
pub const acpi_sdev_namespace = struct_acpi_sdev_namespace;
pub const acpi_sdev_secure_component = struct_acpi_sdev_secure_component;
pub const acpi_sdev_component = struct_acpi_sdev_component;
pub const AcpiSacType = enum_AcpiSacType;
pub const acpi_sdev_id_component = struct_acpi_sdev_id_component;
pub const acpi_sdev_mem_component = struct_acpi_sdev_mem_component;
pub const acpi_sdev_pcie = struct_acpi_sdev_pcie;
pub const acpi_sdev_pcie_path = struct_acpi_sdev_pcie_path;
pub const acpi_table_svkl = struct_acpi_table_svkl;
pub const acpi_svkl_key = struct_acpi_svkl_key;
pub const acpi_svkl_type = enum_acpi_svkl_type;
pub const acpi_svkl_format = enum_acpi_svkl_format;
pub const acpi_table_tdel = struct_acpi_table_tdel;
pub const acpi_table_slic = struct_acpi_table_slic;
pub const acpi_table_slit = struct_acpi_table_slit;
pub const acpi_table_spcr = struct_acpi_table_spcr;
pub const acpi_table_spmi = struct_acpi_table_spmi;
pub const AcpiSpmiInterfaceTypes = enum_AcpiSpmiInterfaceTypes;
pub const acpi_table_srat = struct_acpi_table_srat;
pub const AcpiSratType = enum_AcpiSratType;
pub const acpi_srat_cpu_affinity = struct_acpi_srat_cpu_affinity;
pub const acpi_srat_mem_affinity = struct_acpi_srat_mem_affinity;
pub const acpi_srat_x2apic_cpu_affinity = struct_acpi_srat_x2apic_cpu_affinity;
pub const acpi_srat_gicc_affinity = struct_acpi_srat_gicc_affinity;
pub const acpi_srat_gic_its_affinity = struct_acpi_srat_gic_its_affinity;
pub const acpi_srat_generic_affinity = struct_acpi_srat_generic_affinity;
pub const acpi_table_stao = struct_acpi_table_stao;
pub const acpi_table_tcpa_hdr = struct_acpi_table_tcpa_hdr;
pub const acpi_table_tcpa_client = struct_acpi_table_tcpa_client;
pub const acpi_table_tcpa_server = struct_acpi_table_tcpa_server;
pub const acpi_table_tpm23 = struct_acpi_table_tpm23;
pub const acpi_tmp23_trailer = struct_acpi_tmp23_trailer;
pub const acpi_table_tpm2 = struct_acpi_table_tpm2;
pub const acpi_tpm2_trailer = struct_acpi_tpm2_trailer;
pub const acpi_tpm2_arm_smc = struct_acpi_tpm2_arm_smc;
pub const acpi_table_uefi = struct_acpi_table_uefi;
pub const acpi_table_viot = struct_acpi_table_viot;
pub const acpi_viot_header = struct_acpi_viot_header;
pub const AcpiViotNodeType = enum_AcpiViotNodeType;
pub const acpi_viot_pci_range = struct_acpi_viot_pci_range;
pub const acpi_viot_mmio = struct_acpi_viot_mmio;
pub const acpi_viot_virtio_iommu_pci = struct_acpi_viot_virtio_iommu_pci;
pub const acpi_viot_virtio_iommu_mmio = struct_acpi_viot_virtio_iommu_mmio;
pub const acpi_table_waet = struct_acpi_table_waet;
pub const acpi_table_wdat = struct_acpi_table_wdat;
pub const acpi_wdat_entry = struct_acpi_wdat_entry;
pub const AcpiWdatActions = enum_AcpiWdatActions;
pub const AcpiWdatInstructions = enum_AcpiWdatInstructions;
pub const acpi_table_wddt = struct_acpi_table_wddt;
pub const acpi_table_wdrt = struct_acpi_table_wdrt;
pub const acpi_table_wpbt = struct_acpi_table_wpbt;
pub const acpi_wpbt_unicode = struct_acpi_wpbt_unicode;
pub const acpi_table_wsmt = struct_acpi_table_wsmt;
pub const acpi_table_xenv = struct_acpi_table_xenv;
pub const MALLOC_NODE_FLAGS = enum_MALLOC_NODE_FLAGS;
pub const heap_node = struct_heap_node;
pub const heap_node_buffer = struct_heap_node_buffer;
pub const memory_allocator = struct_memory_allocator;
pub const refc = struct_refc;
pub const hashmap_entry = struct_hashmap_entry;
pub const __hashmap = struct___hashmap;
pub const manifest_dependency = struct_manifest_dependency;
pub const multiboot_header = struct_multiboot_header;
pub const multiboot_header_tag = struct_multiboot_header_tag;
pub const multiboot_header_tag_information_request = struct_multiboot_header_tag_information_request;
pub const multiboot_header_tag_address = struct_multiboot_header_tag_address;
pub const multiboot_header_tag_entry_address = struct_multiboot_header_tag_entry_address;
pub const multiboot_header_tag_console_flags = struct_multiboot_header_tag_console_flags;
pub const multiboot_header_tag_framebuffer = struct_multiboot_header_tag_framebuffer;
pub const multiboot_header_tag_module_align = struct_multiboot_header_tag_module_align;
pub const multiboot_header_tag_relocatable = struct_multiboot_header_tag_relocatable;
pub const multiboot_color = struct_multiboot_color;
pub const multiboot_mmap_entry = struct_multiboot_mmap_entry;
pub const multiboot_tag = struct_multiboot_tag;
pub const multiboot_tag_string = struct_multiboot_tag_string;
pub const multiboot_tag_module = struct_multiboot_tag_module;
pub const multiboot_tag_basic_meminfo = struct_multiboot_tag_basic_meminfo;
pub const multiboot_tag_bootdev = struct_multiboot_tag_bootdev;
pub const multiboot_tag_mmap = struct_multiboot_tag_mmap;
pub const multiboot_vbe_info_block = struct_multiboot_vbe_info_block;
pub const multiboot_vbe_mode_info_block = struct_multiboot_vbe_mode_info_block;
pub const multiboot_tag_vbe = struct_multiboot_tag_vbe;
pub const multiboot_tag_framebuffer_common = struct_multiboot_tag_framebuffer_common;
pub const multiboot_tag_framebuffer = struct_multiboot_tag_framebuffer;
pub const multiboot_tag_elf_sections = struct_multiboot_tag_elf_sections;
pub const multiboot_tag_apm = struct_multiboot_tag_apm;
pub const multiboot_tag_efi32 = struct_multiboot_tag_efi32;
pub const multiboot_tag_efi64 = struct_multiboot_tag_efi64;
pub const multiboot_tag_smbios = struct_multiboot_tag_smbios;
pub const multiboot_tag_old_acpi = struct_multiboot_tag_old_acpi;
pub const multiboot_tag_new_acpi = struct_multiboot_tag_new_acpi;
pub const multiboot_tag_network = struct_multiboot_tag_network;
pub const multiboot_tag_efi_mmap = struct_multiboot_tag_efi_mmap;
pub const multiboot_tag_efi32_ih = struct_multiboot_tag_efi32_ih;
pub const multiboot_tag_efi64_ih = struct_multiboot_tag_efi64_ih;
pub const multiboot_tag_load_base_addr = struct_multiboot_tag_load_base_addr;
pub const multiboot_tag_kflags = struct_multiboot_tag_kflags;
pub const kmem_info = struct_kmem_info;
pub const kmem_region = struct_kmem_region;
pub const kmem_page = struct_kmem_page;
pub const kmem_mapping = struct_kmem_mapping;
pub const partitioned_disk_dev = struct_partitioned_disk_dev;
pub const gpt_partition_header = struct_gpt_partition_header;
pub const gpt_table = struct_gpt_table;
pub const mbr_table = struct_mbr_table;
pub const disk_dev = struct_disk_dev;
pub const gpt_partition_type = struct_gpt_partition_type;
pub const gpt_partition = struct_gpt_partition;
pub const gpt_partition_entry = struct_gpt_partition_entry;
