DRIVER_OUT=$(OUT)/drivers
DRIVER_SRC=$(SRC)/drivers

INPUT_DRIVERS := \
	./input/i8042

ACPI_DRIVERS := \
	./acpi/acpicore

# Define where the drivers are located
DRIVER_PATHS := \
	./env/login \
	./env/kterm \
	./env/lwnd \
	./video/nvidia \
	./video/radeon \
	./debug/test \
	$(INPUT_DRIVERS) \
	$(ACPI_DRIVERS)


build: $(DRIVER_PATHS)

$(DRIVER_PATHS):
	@make build \
		DRIVER_NAME=$(lastword $(subst /, ,$@)) \
		THIS_OUT=$(DRIVER_OUT)/$@ \
		THIS_SRC=$(DRIVER_SRC)/$@ \
		--file=./driver.mk

.PHONY: build $(DRIVER_PATHS)
