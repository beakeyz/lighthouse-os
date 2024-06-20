pub const _abi = @import("anv_abi.zig");

pub const zig_driver = ?*_abi.drv_manifest;
pub const zig_driver_template = _abi.aniva_driver_t;

pub const ZIG_DRVTYPE_DISK = _abi.DT_DISK;
pub const ZIG_DRVTYPE_FS = _abi.DT_FS;
pub const ZIG_DRVTYPE_IO = _abi.DT_IO;
pub const ZIG_DRVTYPE_SOUND = _abi.DT_SOUND;
pub const ZIG_DRVTYPE_GRAPHICS = _abi.DT_GRAPHICS;
pub const ZIG_DRVTYPE_OTHER = _abi.DT_OTHER;
pub const ZIG_DRVTYPE_FIRMWARE = _abi.DT_FIRMWARE;

pub fn driver_version(maj: u8, min: u8, bump: u16) _abi.driver_version_t {
    return _abi.driver_version_t{
        .version = (@as(u32, bump) << 16) | (@as(u32, min) << 8) | maj,
    };
}
