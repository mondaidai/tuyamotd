SDK_DIR := vendor/tuya-iot-core-sdk
SDK_BUILD := $(SDK_DIR)/build
SRC_DIR := src
BUILD_DIR := build
PATCH_STAMP := $(BUILD_DIR)/.patches-applied
PATCH_FILES := patches/010-make_dynamic_lib.patch patches/020-fix_user_data.patch

.PHONY: all submodules patches sdk app clean

all: app

submodules:
	git submodule update --init --recursive --force

patches: $(PATCH_STAMP)

$(PATCH_STAMP): submodules $(PATCH_FILES)
	@set -eu; \
	for patch_file in $(PATCH_FILES); do \
		if git -C $(SDK_DIR) apply --check -p1 "$(CURDIR)/$$patch_file" 2>/dev/null; then \
			git -C $(SDK_DIR) apply -p1 "$(CURDIR)/$$patch_file"; \
		elif git -C $(SDK_DIR) apply --reverse --check -p1 "$(CURDIR)/$$patch_file" 2>/dev/null; then \
			printf '%s already applied\n' "$$patch_file"; \
		else \
			printf 'cannot apply %s\n' "$$patch_file" >&2; \
			exit 1; \
		fi; \
	done
	@mkdir -p $(BUILD_DIR)
	@touch $@

sdk: patches
	cmake -S $(SDK_DIR) -B $(SDK_BUILD) -DBUILD_SHARED_LIBS=ON
	cmake --build $(SDK_BUILD)

app: sdk
	$(MAKE) -C $(SRC_DIR) SDK_DIR=../$(SDK_DIR)

clean:
	$(MAKE) -C $(SRC_DIR) clean
	rm -f $(PATCH_STAMP)
	@if [ -f $(SDK_BUILD)/Makefile ]; then \
		$(MAKE) -C $(SDK_BUILD) clean; \
	fi