ZIG_DRV_ABI_OUT=anvdrv_abi.zig

genbindings:
	@zig translate-c -I./../src/aniva -I./../src ./driver_bindings.h > $(ZIG_DRV_ABI_OUT)

.PHONY: genbindings
