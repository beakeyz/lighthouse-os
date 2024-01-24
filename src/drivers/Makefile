DRIVER_OUT=$(OUT)/drivers
DRIVER_SRC=$(SRC)/drivers

# Define where the drivers are located
DRIVER_PATHS := \
	./env/login \
	./util/kterm \
	./video/nvidia \
	./video/radeon \
	./test \
	./lwnd

build: $(DRIVER_PATHS)

$(DRIVER_PATHS):
	@make build \
		DRIVER_NAME=$(lastword $(subst /, ,$@)) \
		THIS_OUT=$(DRIVER_OUT)/$@ \
		THIS_SRC=$(DRIVER_SRC)/$@ \
		--file=./driver.mk

.PHONY: build $(DRIVER_PATHS)
