DIRECTORY_GUARD=mkdir -p $(@D)

override MODULES_LDFLAGS := \
	-nostdlib \
	-z max-page-size=0x1000 \
	-m elf_x86_64 \
	-static \
	-r \
	--no-dynamic-linker \
	-z text

override MODULES_CFLAGS := \
	-std=gnu11 \
	-nostdlib \
	-Wall \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fshort-wchar \
	-fno-lto \
	-fPIC \
	-shared \
	-m64 \
	-march=x86-64 \
	-mabi=sysv \
	-mno-80387 \
	-mno-mmx \
	-mno-red-zone \
	-MMD \
	-DGNU_EFI_USE_MS_ABI \
	-I. \

# Name defs
MODULES_OBJ=modules.obj

# Define lightloader directories
MODULES_OUT=$(OUT)/modules
MODULES_SRC=$(SRC)/modules

C_SRC := $(shell find $(MODULES_SRC) -type f -name '*.c')
ASM_SRC := $(shell find $(MODULES_SRC) -type f -name '*.asm')
S_SRC := $(shell find $(MODULES_SRC) -type f -name '*.S')

C_OBJ := $(patsubst %.c,%.o,$(subst $(MODULES_SRC),$(MODULES_OUT),$(C_SRC)))
ASM_OBJ := $(patsubst %.asm,%.o,$(subst $(MODULES_SRC),$(MODULES_OUT),$(ASM_SRC)))
S_OBJ := $(patsubst %.S,%.o,$(subst $(MODULES_SRC),$(MODULES_OUT),$(S_SRC)))

override HEADER_DEPS := $(patsubst %.o,%.d,$(C_OBJ))

-include $(HEADER_DEPS)
$(MODULES_OUT)/%.o: $(MODULES_SRC)/%.c
	@echo -e - Building [C]: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(MODULES_CFLAGS) -c $< -o $@

$(MODULES_OUT)/%.o: $(MODULES_SRC)/%.S
	@echo -e - Building [S]: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(MODULES_CFLAGS) -c $< -o $@

$(MODULES_OUT)/%.o: $(MODULES_SRC)/%.asm
	@echo -e - Building [ASM]: $@
	@$(DIRECTORY_GUARD)
	@$(ASM) $< -o $@ -f elf64

build: $(C_OBJ) $(ASM_OBJ) $(S_OBJ)
	@echo -e [MODULES] Linking objects...
	@$(LD) $^ $(MODULES_LDFLAGS) -o $(MODULES_OUT)/$(MODULES_OBJ)
	@echo -e [MODULES] Done =D

.PHONY: build
