KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m += ex34.o

all :
	make -C $(KERNELDIR_PNL) M=$(PWD) modules
	gcc -o use_current use_ioctl.c
	gcc -o use_rewind use_rewind.c

clean:
	make -C $(KERNELDIR_PNL) M=$(PWD) clean
	rm use_current use_rewind