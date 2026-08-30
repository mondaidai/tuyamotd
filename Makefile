CC ?= gcc

CFLAGS := -Iinclude

LDFLAGS := \
	-llink_core \
	-lmiddleware_implementation \
	-lplatform_port \
	-lutils_modules \
	-lcjson \
	-largp \
	-lblobmsg_json \
	-lubox \
	-lubus \
	-luci

OBJ_DIR := obj
BUILD_DIR := build
TARGET := tuyamotd

SRCS := \
    $(wildcard src/*.c) \
    $(wildcard src/esp_actions/*.c) \
    $(wildcard src/actions/*.c)

OBJS := $(addprefix $(OBJ_DIR)/,$(SRCS:.c=.o))

.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BUILD_DIR)