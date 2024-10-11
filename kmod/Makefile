TARGET = led-ugreen
obj-m += led-ugreen.o
ccflags-y := -std=gnu11

# if KERNELRELEASE isn't set, i.e. not being built w/ DMKS, then use uname -r
KERNELRELEASE ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KERNELRELEASE)/build
INSTALL_MOD_PATH ?= /

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

install:
	make -C $(KDIR) M=$(PWD) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) modules_install
