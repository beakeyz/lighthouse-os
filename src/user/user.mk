DIRECTORY_GUARD=mkdir -p $(@D)

C_SRC := $(shell find $(THIS_SRC) -type f -name '*.c')
ASM_SRC := $(shell find $(THIS_SRC) -type f -name '*.asm')

C_OBJ := $(patsubst %.c,%.o,$(subst $(THIS_SRC),$(THIS_OUT),$(C_SRC)))
ASM_OBJ := $(patsubst %.asm,%.o,$(subst $(THIS_SRC),$(THIS_OUT),$(ASM_SRC)))

override HEADER_DEPS := $(patsubst %.o,%.d,$(C_OBJ))

-include $(HEADER_DEPS)
$(THIS_OUT)/%.o: $(THIS_SRC)/%.c
	@echo -e - Building [C]: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(USER_CFLAGS) -c $< -o $@

$(THIS_OUT)/%.o: $(THIS_SRC)/%.asm
	@echo -e - Building [ASM]: $@
	@$(DIRECTORY_GUARD)
	@$(ASM) $< -o $@ -f elf64

# TODO: Shared libraries go into the flags, create a variable that creates a flags
# entry for every shared library that the process specifies
build-dynamic: $(ASM_OBJ) $(C_OBJ)
	@$(LD) $^ -o $(THIS_OUT)/$(PROCESS_NAME)$(PROCESS_EXT) \
		$(USER_DYNAMIC_LDFLAGS)

build-static: $(ASM_OBJ) $(C_OBJ)
	$(LD) $^ $(LIBRARIES) -o $(THIS_OUT)/$(PROCESS_NAME)$(PROCESS_EXT) \
		$(USER_STATIC_LDFLAGS)

build:
ifeq ($(LINK_TYPE), dynamic)
	@$(MAKE) build-dynamic
else
	@$(MAKE) build-static
endif
	@echo -e "Built process: " $(PROCESS_NAME)

.PHONY: build-dynamic build-static
