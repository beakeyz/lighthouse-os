export LIBRARY_OUT=$(OUT)/libs
export LIBRARY_SRC=$(SRC)/libs

LIBRARY_PATHS := \
	./librt \
	./LibC \
	./LibGfx

export LIBRARY_SHARED_FILE_EXT := .slb
export LIBRARY_STATIC_FILE_EXT := .lib

export LIBRARY_CFLAGS= -std=gnu11 -Wall -O2 -ffreestanding -fPIC -shared -D'LIBENV' $(USER_INCLUDE_CFLAGS)
export LIBRARY_SHARED_LDFLAGS= -fPIC -shared -nostdlib -L$(SYSROOT_DIR)/System/Lib
export LIBRARY_STATIC_LDFLAGS= -nostdlib -static -r

build: $(LIBRARY_PATHS)

$(LIBRARY_PATHS):
	@make -C $@ build \
		THIS_SRC=$(LIBRARY_SRC)/$@ \
		THIS_OUT=$(LIBRARY_OUT)/$@

.PHONY: build $(LIBRARY_PATHS)
