TARGET = led-ugreen
obj-m += led-ugreen.o
ccflags-y := -std=gnu11

# if KERNELRELEASE isn't set, i.e. not being built w/ DMKS, then use uname -r
# Fall back to available kernel if current kernel build dir doesn't exist
KERNELRELEASE ?= $(shell uname -r)
KDIR_DEFAULT := /lib/modules/$(KERNELRELEASE)/build

# Check if default kernel build directory exists, otherwise find an available one
ifeq ($(wildcard $(KDIR_DEFAULT)),)
    KERNELRELEASE := $(shell find /lib/modules -name "build" -type l | head -n1 | sed 's|/lib/modules/||;s|/build||')
    KDIR := /lib/modules/$(KERNELRELEASE)/build
else
    KDIR := $(KDIR_DEFAULT)
endif

KDIR ?= $(KDIR_DEFAULT)
INSTALL_MOD_PATH ?= /

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

install:
	make -C $(KDIR) M=$(PWD) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) modules_install
