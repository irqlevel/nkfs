include $(PROJ_ROOT)/Makefile.inc

obj-m = ds.o

ccflags-y := -I$(src) -D __KERNEL__ $(COMMON_CFLAGS)

ds-y := ext_tree.o module.o dev.o net.o		\
	super.o balloc.o inode.o btree.o	\
	upages.o ksocket.o route.o dio.o	\

KBUILD_EXTRA_SYMBOLS = $(PROJ_ROOT)/crt/kernel/Module.symvers

ifndef DS_KERNEL_PATH
DS_KERNEL_PATH=/lib/modules/$(shell uname -r)/build
endif

all:
	make -C $(DS_KERNEL_PATH) M=$(PWD) modules
	cp ds.ko ../bin/ds.ko
clean:
	make -C $(DS_KERNEL_PATH) M=$(PWD) clean
	rm -r -f *.o
