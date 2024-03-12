DIRECTORY_GUARD=mkdir -p $(@D)

C_SRC := $(shell find $(THIS_SRC) -type f -name '*.c')
ASM_SRC := $(shell find $(THIS_SRC) -type f -name '*.asm')
# Libraries may have assembly in the intel syntax
# TODO: remove these and make them NASM
S_SRC := $(shell find $(THIS_SRC) -type f -name '*.S')

C_OBJ := $(patsubst %.c,%.o,$(subst $(THIS_SRC),$(THIS_OUT),$(C_SRC)))
ASM_OBJ := $(patsubst %.asm,%.o,$(subst $(THIS_SRC),$(THIS_OUT),$(ASM_SRC)))
S_OBJ ?= $(patsubst %.S,%.o,$(subst $(THIS_SRC),$(THIS_OUT),$(S_SRC)))

override HEADER_DEPS := $(patsubst %.o,%.d,$(C_OBJ))

-include $(HEADER_DEPS)
$(THIS_OUT)/%.o: $(THIS_SRC)/%.c
	@echo -e - Building [C]: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(LIBRARY_CFLAGS) -c $< -o $@

$(THIS_OUT)/%.o: $(THIS_SRC)/%.asm
	@echo -e - Building [ASM]: $@
	@$(DIRECTORY_GUARD)
	@$(ASM) $< -o $@ -f elf64

$(THIS_OUT)/%.o: $(THIS_SRC)/%.S
	@echo -e - Building [S]: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(LIBRARY_CFLAGS) -c $< -o $@

$(LIBRARY_OUT)/%.o: $(LIBRARY_SRC)/%.asm
	@echo -e - Building common lib component [ASM]: $@
	@$(DIRECTORY_GUARD)
	@$(ASM) $< -o $@ -f elf64

#
# TODO: Put libraries in the $(LIBRARY_BIN_PATH) directory
# So change -o $(THIS_OUT)/... -> -o $(LIBRARY_BIN_PATH)/...
#

build-shared: $(ASM_OBJ) $(C_OBJ)
ifeq ($(LIBRARY_NAME), LibC)
	@$(LD) $(LIBRARY_SHARED_LDFLAGS) $^ -o $(THIS_OUT)/$(LIBRARY_NAME)$(LIBRARY_SHARED_FILE_EXT)
else
	@$(LD) $(LIBRARY_SHARED_LDFLAGS) -l:LibC.slb $(DEPS) $^ -o $(THIS_OUT)/$(LIBRARY_NAME)$(LIBRARY_SHARED_FILE_EXT)
endif

build-static: $(ASM_OBJ) $(C_OBJ) $(S_OBJ)
	@$(LD) $^ -o $(THIS_OUT)/$(LIBRARY_NAME)$(LIBRARY_STATIC_FILE_EXT) \
		$(LIBRARY_STATIC_LDFLAGS)

build:
ifeq ($(LINKTYPE), dynamic)
	@$(MAKE) build-static
	@$(MAKE) build-shared
endif
ifeq ($(LINKTYPE), static)
	@$(MAKE) build-static
endif
ifeq ($(LINKTYPE), shared)
	@$(MAKE) build-shared
endif
	@echo "Built lib: " $(LIBRARY_NAME)

.PHONY: build-shared build-libc-shared build-static
