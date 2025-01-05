DIRECTORY_GUARD=mkdir -p $(@D)

override LSDK_LDFLAGS := \
	-nostdlib \
	-z max-page-size=0x1000 \
	-m elf_x86_64 \
	-static \
	-r \
	--no-dynamic-linker \
	-z text

override LSDK_CFLAGS := \
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
	-I$(SRC) \

# Name defs
LSDK_OBJ=lsdk.obj

# Define lightloader directories
LSDK_OUT=$(OUT)/lsdk
LSDK_SRC=$(SRC)/lsdk

C_SRC := $(shell find $(LSDK_SRC) -type f -name '*.c')
ASM_SRC := $(shell find $(LSDK_SRC) -type f -name '*.asm')
S_SRC := $(shell find $(LSDK_SRC) -type f -name '*.S')

C_OBJ := $(patsubst %.c,%.o,$(subst $(LSDK_SRC),$(LSDK_OUT),$(C_SRC)))
ASM_OBJ := $(patsubst %.asm,%.o,$(subst $(LSDK_SRC),$(LSDK_OUT),$(ASM_SRC)))
S_OBJ := $(patsubst %.S,%.o,$(subst $(LSDK_SRC),$(LSDK_OUT),$(S_SRC)))

override HEADER_DEPS := $(patsubst %.o,%.d,$(C_OBJ))

-include $(HEADER_DEPS)
$(LSDK_OUT)/%.o: $(LSDK_SRC)/%.c
	@echo -e - Building [C]: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(LSDK_CFLAGS) -c $< -o $@

$(LSDK_OUT)/%.o: $(LSDK_SRC)/%.S
	@echo -e - Building [S]: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(LSDK_CFLAGS) -c $< -o $@

$(LSDK_OUT)/%.o: $(LSDK_SRC)/%.asm
	@echo -e - Building [ASM]: $@
	@$(DIRECTORY_GUARD)
	@$(ASM) $< -o $@ -f elf64

build: $(C_OBJ) $(ASM_OBJ) $(S_OBJ)
	@echo -e [LSDK] Linking objects...
	@$(LD) $^ $(LSDK_LDFLAGS) -o $(LSDK_OUT)/$(LSDK_OBJ)
	@echo -e [LSDK] Done =D

.PHONY: build
