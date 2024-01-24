USER_OUT=$(OUT)/user
USER_SRC=$(SRC)/user

# Define where the drivers are located
USER_PATHS := \
	./init

# Currently there is no process extention lmao
export PROCESS_EXT=

export USER_CFLAGS=-std=gnu11 -Wall -shared -fPIC -O2 -ffreestanding $(USER_INCLUDE_CFLAGS)
export USER_LDFLAGS=
export USER_STATIC_LDFLAGS=
export USER_DYNAMIC_LDFLAGS=-L{SYSROOT_DIR}/System/Lib

build: $(USER_PATHS)

$(USER_PATHS):
	@echo $(LIBRARY_BIN_PATH)
	@make -C $@ build \
		THIS_SRC=$(USER_SRC)/$@ \
		THIS_OUT=$(USER_OUT)/$@

.PHONY: build $(USER_PATHS)
