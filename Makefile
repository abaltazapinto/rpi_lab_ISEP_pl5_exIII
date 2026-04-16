obj-m += dimmer-rpi4.o
dimmer-rpi4-objs := blinker_hr.o gpio.o

KERNEL_DIR = /lib/modules/$(shell uname -r)/build
CCC_ARGS = ARCH=arm64

all:
	make -C ${KERNEL_DIR} M=$(PWD) modules $(CCC_ARGS)

clean:
	make -C ${KERNEL_DIR} M=$(PWD) clean

