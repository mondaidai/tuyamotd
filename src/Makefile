CC := gcc
SDK_DIR ?= ../vendor/tuya-iot-core-sdk
CFLAGS := -I../include -I$(SDK_DIR)/include -I$(SDK_DIR)/utils -I$(SDK_DIR)/interface
LDFLAGS := \
	-L$(SDK_DIR)/build/lib \
	-Wl,-rpath,'$$ORIGIN/$(SDK_DIR)/build/lib' \
	-llink_core \
	-lmiddleware_implementation \
	-lplatform_port \
	-lutils_modules
SRC_DIR := .
OBJ_DIR := ../obj
BUILD_DIR := ../build

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

TARGET := tuyamotd

.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(BUILD_DIR)/$(TARGET)