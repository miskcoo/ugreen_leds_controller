TARGET = led-ugreen
obj-m += led-ugreen.o
ccflags-y := -std=gnu11

# if KERNELRELEASE isn't set, i.e. not being built w/ DMKS, then use uname -r
KERNELRELEASE ?= $(shell uname -r)

all:
	make -C /lib/modules/$(KERNELRELEASE)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(KERNELRELEASE)/build M=$(PWD) clean
