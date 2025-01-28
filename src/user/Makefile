USER_OUT=$(OUT)/user
USER_SRC=$(SRC)/user

# Define where the userapp sources are located
USER_PATHS := ./init \
			  ./ls \
			  ./cat \
			  ./todo

# Currently there is no process extention lmao
export PROCESS_EXT=

export USER_CFLAGS=-std=gnu11 -Wall -shared -fPIC -ffreestanding -O2 $(USER_INCLUDE_CFLAGS)
export USER_LDFLAGS=
export USER_STATIC_LDFLAGS=
export USER_DYNAMIC_LDFLAGS=-Idynldr.drv

build: $(USER_PATHS)

$(USER_PATHS):
	@echo $(LIBRARY_BIN_PATH)
	@make -j$(nproc) -C $@ build \
		THIS_SRC=$(USER_SRC)/$@ \
		THIS_OUT=$(USER_OUT)/$@

.PHONY: build $(USER_PATHS)
