const std = @import("std");
const drv = std.driver;

export var ___lightos_driver = drv.zig_driver_template{
    .m_name = "zig_test",
    .m_descriptor = "Idk lmao",
    .m_type = drv.ZIG_DRVTYPE_OTHER,
    .m_version = drv.driver_version(0, 2, 1),
    .f_init = drv_init,
};

pub fn drv_init(_: drv.zig_driver) callconv(.C) c_int {
    return 0;
}
