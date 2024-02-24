ZIG_MAIN_ABI_OUT=anv_abi.zig

genbindings:
	@zig translate-c -I./../src/aniva -I./../src ./bindings.h > $(ZIG_MAIN_ABI_OUT)

.PHONY: genbindings
