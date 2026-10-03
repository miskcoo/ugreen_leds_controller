CLANG ?= clang
LLVM_LINK ?= llvm-link
LLC ?= llc
CC ?= gcc
PKG_CONFIG ?= pkg-config

MULTIARCH ?= $(shell $(CC) -dumpmachine)
ARCH ?= x86

INCLUDE_DIR := include
SOURCE_DIR := src
TEST_DIR := tests
BUILD_DIR := build

BPF_SOURCES := bpf/maps.bpf.c bpf/network.bpf.c bpf/disk.bpf.c
BPF_BITCODE := $(patsubst bpf/%.c,$(BUILD_DIR)/bpf/%.bc,$(BPF_SOURCES))
BPF_CFLAGS := -O2 -g -target bpf -D__TARGET_ARCH_$(ARCH) \
              -I/usr/include/$(MULTIARCH) -I$(INCLUDE_DIR) -Ibpf

USER_SOURCES := $(SOURCE_DIR)/main.c $(SOURCE_DIR)/config.c \
                $(SOURCE_DIR)/scheduler.c $(SOURCE_DIR)/led_controller.c \
                $(SOURCE_DIR)/network_led.c $(SOURCE_DIR)/disk_led.c \
                $(SOURCE_DIR)/disk_layout.c \
                $(SOURCE_DIR)/power_led.c $(SOURCE_DIR)/bpf_runtime.c
USER_OBJECTS := $(patsubst $(SOURCE_DIR)/%.c,$(BUILD_DIR)/src/%.o,$(USER_SOURCES))
USER_CFLAGS := -std=gnu11 -O2 -g -Wall -Wextra -Wpedantic \
               -I$(INCLUDE_DIR) -I$(SOURCE_DIR) \
               $(shell $(PKG_CONFIG) --cflags libbpf)
USER_LIBS := $(shell $(PKG_CONFIG) --libs libbpf)

TEST_OBJECTS := $(BUILD_DIR)/tests/ugreen_ledd_test.o \
                $(BUILD_DIR)/tests/config.o $(BUILD_DIR)/tests/scheduler.o \
                $(BUILD_DIR)/tests/disk_layout.o

.PHONY: all test clean

all: ugreen_led.bpf.o ugreen-ledd

$(BUILD_DIR)/bpf $(BUILD_DIR)/src $(BUILD_DIR)/tests:
	mkdir -p $@

$(BUILD_DIR)/bpf/%.bpf.bc: bpf/%.bpf.c bpf/maps.bpf.h $(INCLUDE_DIR)/ugreen_bpf_shared.h | $(BUILD_DIR)/bpf
	$(CLANG) $(BPF_CFLAGS) -emit-llvm -c $< -o $@

ugreen_led.bpf.o: $(BPF_BITCODE)
	$(LLVM_LINK) $^ -o $(BUILD_DIR)/bpf/ugreen_led.bc
	$(LLC) -march=bpf -filetype=obj $(BUILD_DIR)/bpf/ugreen_led.bc -o $@

$(BUILD_DIR)/src/%.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR)/src
	$(CC) $(USER_CFLAGS) -MMD -MP -c $< -o $@

ugreen-ledd: $(USER_OBJECTS)
	$(CC) $^ -o $@ $(USER_LIBS)

$(BUILD_DIR)/tests/ugreen_ledd_test.o: $(TEST_DIR)/ugreen_ledd_test.c | $(BUILD_DIR)/tests
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/config.o: $(SOURCE_DIR)/config.c | $(BUILD_DIR)/tests
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/scheduler.o: $(SOURCE_DIR)/scheduler.c | $(BUILD_DIR)/tests
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/disk_layout.o: $(SOURCE_DIR)/disk_layout.c | $(BUILD_DIR)/tests
	$(CC) $(USER_CFLAGS) -c $< -o $@

ugreen-ledd-test: $(TEST_OBJECTS)
	$(CC) $^ -o $@

test: ugreen-ledd-test
	./ugreen-ledd-test

ugreen-bpf-load-test: $(TEST_DIR)/bpf_load_test.c
	$(CC) $(USER_CFLAGS) $< -o $@ $(USER_LIBS)

clean:
	rm -rf $(BUILD_DIR) ugreen_led.bpf.o ugreen-ledd ugreen-ledd-test \
		ugreen-bpf-load-test

-include $(USER_OBJECTS:.o=.d)
