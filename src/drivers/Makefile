DRIVER_OUT=$(OUT)/drivers
DRIVER_SRC=$(SRC)/drivers

DISK_DRIVERS := 	\
	./disk/usb/usbdisk 

INPUT_DRIVERS := 	\
	./input/i8042 	\
	./input/usbkbd	\
	./input/usbmouse

ACPI_DRIVERS := \
	./acpi/acpicore \
	./acpi/acpidev

USB_DRIVERS := \
	./usb/usbcore \
	./usb/ehci \
	./usb/uhci \
	./usb/xhci

# Define where the drivers are located
DRIVER_PATHS := \
	./env/login \
	./env/kterm \
	./env/lwnd \
	./env/dynldr \
	./env/prism \
	./env/upi \
	./video/nvidia \
	./video/radeon \
	./debug/test \
	$(DISK_DRIVERS) \
	$(USB_DRIVERS) \
	$(INPUT_DRIVERS) \
	$(ACPI_DRIVERS)


build: $(DRIVER_PATHS)

$(DRIVER_PATHS):
	@make -j$(nproc) build \
		DRIVER_NAME=$(lastword $(subst /, ,$@)) \
		THIS_OUT=$(DRIVER_OUT)/$@ \
		THIS_SRC=$(DRIVER_SRC)/$@ \
		--file=./driver.mk

.PHONY: build $(DRIVER_PATHS)
