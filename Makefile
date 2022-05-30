obj-m += ouichefs.o
ouichefs-objs := fs.o super.o inode.o file.o dir.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
IOCTL_SUB = ioctl
MKFS_SUB = mkfs
all:
	make -C $(KERNELDIR) M=$(PWD) modules
	cd $(IOCTL_SUB) && $(MAKE)
	cd $(MKFS_SUB) && $(MAKE)
debug:
	make -C $(KERNELDIR) M=$(PWD) ccflags-y+="-DDEBUG -g" modules
	cd $(IOCTL_SUB) && $(MAKE) ccflags-y+="-DDEBUG -g"
	cd $(MKFS_SUB) && $(MAKE) ccflags-y+="-DDEBUG -g"
clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	cd $(IOCTL_SUB) && $(MAKE) clean
	cd $(MKFS_SUB) && $(MAKE) clean
	rm -rf *~

.PHONY: all clean
