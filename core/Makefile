include $(PROJECT_ROOT)/Makefile.inc

NKFS_MOD = nkfs
NKFS_MOD_KO = $(NKFS_MOD).ko

obj-m = $(NKFS_MOD).o

ccflags-y := -I$(src) -D __KERNEL__ $(PROJECT_CFLAGS) $(PROJECT_EXTRA_CFLAGS)

$(NKFS_MOD)-y := module.o dev.o net.o			\
	super.o balloc.o inode.o btree.o		\
	trace.o upages.o ksocket.o route.o dio.o	\

KBUILD_EXTRA_SYMBOLS = $(PROJECT_ROOT)/crt/kernel/Module.symvers

all:
	$(MAKE) -C $(KERNEL_BUILD_PATH) M=$(PROJECT_ROOT)/core modules
	cp $(NKFS_MOD_KO) ../bin/$(NKFS_MOD_KO)
clean:
	$(MAKE) -C $(KERNEL_BUILD_PATH) M=$(PROJECT_ROOT)/core clean
	rm -r -f *.o
