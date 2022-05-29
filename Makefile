obj-m += ouichefs.o
ouichefs-objs := fs.o super.o inode.o file.o dir.o

KERNELDIR ?= /home/maigh/Studies/M1_S2/PNL/TME/kernel/build/linux-5.10.17
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
