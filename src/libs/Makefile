export LIBRARY_OUT=$(OUT)/libs
export LIBRARY_SRC=$(SRC)/libs

# Excluded libraries:
#  - raylib: Until we feature an OpenGL implementation
LIBRARY_PATHS := \
	./lightos \
	./librt \
	./libgfx \
	./lightui \
	./kterm

export LIBRARY_SHARED_FILE_EXT := .lib
export LIBRARY_STATIC_FILE_EXT := .obj

export LIBRARY_CFLAGS= -std=gnu11 -Wall -O2 -ffreestanding -fPIC -shared -D'LIBENV' $(USER_INCLUDE_CFLAGS)
export LIBRARY_SHARED_LDFLAGS= -fPIC -shared -L$(SYSROOT_LIBRARY_DIR)
export LIBRARY_STATIC_LDFLAGS= -static -r

build: $(LIBRARY_PATHS)

$(LIBRARY_PATHS):
	@make -j$(nproc) -C $@ build \
		THIS_SRC=$(LIBRARY_SRC)/$@ \
		THIS_OUT=$(LIBRARY_OUT)/$@

.PHONY: build $(LIBRARY_PATHS)
