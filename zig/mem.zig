const abi = @import("anv_abi.zig");

pub fn kmalloc(size: usize) !*void {
    return abi.kmalloc(size);
}
