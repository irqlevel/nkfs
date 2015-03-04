include $(PROJ_ROOT)/Makefile.inc

NKFS_MOD = nkfs
NKFS_MOD_KO = $(NKFS_MOD).ko

obj-m = $(NKFS_MOD).o

ccflags-y := -I$(src) -D __KERNEL__ $(COMMON_CFLAGS)

CCFLAGS_inode.o = -I./inc

$(NKFS_MOD)-y := module.o dev.o net.o		\
	super.o balloc.o inode.o btree.o	\
	upages.o ksocket.o route.o dio.o	\

KBUILD_EXTRA_SYMBOLS = $(PROJ_ROOT)/crt/kernel/Module.symvers

ifndef NKFS_KERNEL_PATH
NKFS_KERNEL_PATH=/lib/modules/$(shell uname -r)/build
endif

all:
	$(MAKE) -C $(NKFS_KERNEL_PATH) M=$(PROJ_ROOT)/kmod modules
	cp $(NKFS_MOD_KO) ../bin/$(NKFS_MOD_KO)
clean:
	$(MAKE) -C $(NKFS_KERNEL_PATH) M=$(PROJ_ROOT)/kmod clean
	rm -r -f *.o
