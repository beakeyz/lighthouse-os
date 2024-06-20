DRIVER_OUT=$(OUT)/drivers
DRIVER_SRC=$(SRC)/drivers

INPUT_DRIVERS := \
	./input/inptcore \
	./input/i8042 \
	./input/usbkbd

ACPI_DRIVERS := \
	./acpi/acpicore \
	./acpi/acpidev

USB_DRIVERS := \
	./usb/usbcore \
	./usb/ehci \
	./usb/xhci

# Define where the drivers are located
DRIVER_PATHS := \
	./env/login \
	./env/kterm \
	./env/lwnd \
	./env/dynldr \
	./env/prism \
	./video/nvidia \
	./video/radeon \
	./debug/test \
	$(USB_DRIVERS) \
	$(INPUT_DRIVERS) \
	$(ACPI_DRIVERS)

ZIG_DRIVERS := \
	./debug/zigtest

build: $(DRIVER_PATHS) $(ZIG_DRIVERS)

$(DRIVER_PATHS):
	@make build \
		DRIVER_NAME=$(lastword $(subst /, ,$@)) \
		THIS_OUT=$(DRIVER_OUT)/$@ \
		THIS_SRC=$(DRIVER_SRC)/$@ \
		--file=./driver.mk

$(ZIG_DRIVERS):
	@make build \
		DRIVER_NAME=$(lastword $(subst /, ,$@)) \
		THIS_OUT=$(DRIVER_OUT)/$@ \
		THIS_SRC=$(DRIVER_SRC)/$@ \
		--file=./driver_zig.mk

.PHONY: build $(DRIVER_PATHS) $(ZIG_DRIVERS)
