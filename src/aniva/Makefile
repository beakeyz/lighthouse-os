DIRECTORY_GUARD=mkdir -p $(@D)

KERNEL_OUT=$(OUT)/$(KERNEL_NAME)
KERNEL_SRC=$(SRC)/$(KERNEL_NAME)
KERNEL_LINKERSCRIPT_PATH=$(KERNEL_SRC)/entry/linker.ld

C_SRC := $(shell find $(KERNEL_SRC) -type f -name '*.c')
ASM_SRC := $(shell find $(KERNEL_SRC) -type f -name '*.asm')

C_OBJ := $(patsubst %.c,%.o,$(subst $(KERNEL_SRC),$(KERNEL_OUT),$(C_SRC)))
ASM_OBJ := $(patsubst %.asm,%.o,$(subst $(KERNEL_SRC),$(KERNEL_OUT),$(ASM_SRC)))

override HEADER_DEPS := $(patsubst %.o,%.d,$(C_OBJ))
override CFLAGS=$(KERNEL_CFLAGS)
override LDFLAGS=$(KERNEL_LDFLAGS)

-include $(HEADER_DEPS)
$(KERNEL_OUT)/%.o: %.c
	@echo -e Building [C]: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_OUT)/%.o: %.asm
	@echo -e Building [ASM]: $@
	@$(DIRECTORY_GUARD)
	@$(ASM) $< -o $@ -f elf64


## NOTE: We need to invoke a script that checks if ksyms.asm exists
build: $(ASM_OBJ) $(C_OBJ)
	@cd $(PROJECT_DIR)/.. && python3 $(PROJECT_DIR)/x.py link

