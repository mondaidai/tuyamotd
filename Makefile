SDK_DIR := vendor/tuya-iot-core-sdk
SDK_BUILD := $(SDK_DIR)/build
SRC_DIR := src

.PHONY: all sdk app clean

all: sdk app

sdk:
	cmake -S $(SDK_DIR) -B $(SDK_BUILD) -DBUILD_SHARED_LIBS=ON
	cmake --build $(SDK_BUILD)

app:
	$(MAKE) -C $(SRC_DIR) SDK_DIR=../$(SDK_DIR)

clean:
	$(MAKE) -C $(SRC_DIR) clean
	@if [ -f $(SDK_BUILD)/Makefile ]; then \
		$(MAKE) -C $(SDK_BUILD) clean; \
	fi