TARGET = led-ugreen
obj-m += led-ugreen.o
ccflags-y := -std=gnu11

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
