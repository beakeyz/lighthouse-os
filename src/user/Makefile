USER_OUT=$(OUT)/user
USER_SRC=$(SRC)/user

# These are fancy admin core programs
ADMIN_CORE_PROCS := 	\
	./init 				\
	./sysconf

# Define where the userapp sources are located
USER_PATHS := 			\
	$(ADMIN_CORE_PROCS) \
	./test 				\
	./doom				\
	./gfx_test			\
	./ls				\
	./kill				\
	./vaseprob			\
	./mndlbrt			\
	./diskutil

# Currently there is no process extention lmao
export PROCESS_EXT=

export USER_CFLAGS=-std=gnu11 -Wall -shared -fPIC -O2 -ffreestanding $(USER_INCLUDE_CFLAGS)
export USER_LDFLAGS=
export USER_STATIC_LDFLAGS=
export USER_DYNAMIC_LDFLAGS=-rpath-link $(SYSROOT_DIR)/System/Lib -L$(SYSROOT_DIR)/System/Lib -Idynldr.drv -e main 

build: $(USER_PATHS)

$(USER_PATHS):
	@echo $(LIBRARY_BIN_PATH)
	@make -C $@ build \
		THIS_SRC=$(USER_SRC)/$@ \
		THIS_OUT=$(USER_OUT)/$@

.PHONY: build $(USER_PATHS)
