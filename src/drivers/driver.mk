#
# Template for building any driver in this directory
# Simply gets called from the main Makefile with 'make -f driver.mk'
#
# This is possible because all drivers have a uniform structure: they're all relocatable
# objects that get linked directly against the kernel
#
# The following variables are exported from the underlying makefile:
# DRIVER_NAME: The rule is that any driver that gets built will get the 
#			   name of the directory its in
# THIS_OUT: The output directory for this driver
# THIS_SRC: The source directory for this driver
#

DIRECTORY_GUARD=mkdir -p $(@D)

C_SRC := $(shell find $(THIS_SRC) -type f -name '*.c')
ASM_SRC := $(shell find $(THIS_SRC) -type f -name '*.asm')

C_OBJ := $(patsubst %.c,%.o,$(subst $(THIS_SRC),$(THIS_OUT),$(C_SRC)))
ASM_OBJ := $(patsubst %.asm,%.o,$(subst $(THIS_SRC),$(THIS_OUT),$(ASM_SRC)))

override HEADER_DEPS := $(patsubst %.o,%.d,$(C_OBJ))

# Make sure we define the buildflags
override DRIVER_CFLAGS=$(KERNEL_CFLAGS)
override DRIVER_LDFLAGS= -r

-include $(HEADER_DEPS)
$(THIS_OUT)/%.o: $(THIS_SRC)/%.c
	@echo -e - Building [C]: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(DRIVER_CFLAGS) -c $< -o $@

$(THIS_OUT)/%.o: $(THIS_SRC)/%.asm
	@echo -e - Building [ASM]: $@
	@$(DIRECTORY_GUARD)
	@$(ASM) $< -o $@ -f elf64


## NOTE: We need to invoke a script that checks if ksyms.asm exists
build: $(ASM_OBJ) $(C_OBJ)
	@$(LD) $^ -o $(THIS_OUT)/$(DRIVER_NAME).drv $(DRIVER_LDFLAGS)
