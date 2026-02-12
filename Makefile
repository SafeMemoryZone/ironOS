# Directories and files
OUT ?= ironOS
BUILD_DIR ?= build

SRC_DIR := src
INC_DIR := include
LINKER_SCRIPTS_DIR := linker-scripts

# Export environment variables needed for package.sh
export OUT
export BUILD_DIR
export ISO_NAME := $(OUT).iso

# Compiler and linker definitions
CC := $(TOOLCHAIN_PREFIX)gcc
LD := $(TOOLCHAIN_PREFIX)ld

# Internal compile flags
CFLAGS += \
    -Wall \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fno-PIC \
    -ffunction-sections \
    -fdata-sections \
    -m64 \
    -march=x86-64 \
    -mabi=sysv \
    -mno-80387 \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel

CPPFLAGS += \
    -I include \
    -MMD \
    -MP

LDFLAGS += \
    -m elf_x86_64 \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    --gc-sections \
    -T $(LINKER_SCRIPTS_DIR)/linker.ld

# Source files
SRCS := $(shell find $(SRC_DIR) -type f \( -name '*.c' -o -name '*.S' -o -name '*.asm' \))
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
-include $(DEPS)

# Rules
.PHONY: all clean

all: $(BUILD_DIR)/$(ISO_NAME)

$(BUILD_DIR)/$(ISO_NAME): $(BUILD_DIR)/$(OUT)
	./package.sh

$(BUILD_DIR)/$(OUT): $(OBJS)
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.S.o: %.S
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
