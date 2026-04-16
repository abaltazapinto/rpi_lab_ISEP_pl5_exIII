obj-m += blinker-rpi4.o
blinker-rpi4-objs := blinker.o gpio.o

KERNEL_DIR = /lib/modules/$(shell uname -r)/build
CCC_ARGS = ARCH=arm64

all:
	make -C ${KERNEL_DIR} M=$(PWD) modules $(CCC_ARGS)

clean:
	make -C ${KERNEL_DIR} M=$(PWD) clean

