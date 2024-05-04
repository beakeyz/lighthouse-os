const abi = @import("anv_abi.zig");

const driver = abi.aniva_driver_t{
    .m_name = "zig_test",
    .m_descriptor = "Idk lmao",
    .f_init = drv_init,
};

pub fn drv_init() !void {
    abi.printf("Yay\n");
}

// Exported function which should give a driver?
export fn los_driver_entry() !*abi.aniva_driver_t {
    return &driver;
}
