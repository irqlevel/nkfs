include $(PROJ_ROOT)/Makefile.inc

obj-m = ds.o

ccflags-y := -I$(src) -g3 -D __KERNEL__ $(COMMON_CFLAGS)

ds-y := module.o dev_io.o dev.o net.o random.o vfile.o \
	super.o balloc.o inode.o crt.o btree.o amap.o \
	upages.o klog.o ksocket.o \
	crtlib.o_shipped

ifndef DS_KERNEL_PATH
DS_KERNEL_PATH=/lib/modules/$(shell uname -r)/build
endif

all:
	cp $(PROJ_ROOT)/libs/crtlib_km.a crtlib.o_shipped
	make -C $(DS_KERNEL_PATH) M=$(PWD) modules
	cp ds.ko ../bin/ds.ko
clean:
	make -C $(DS_KERNEL_PATH) M=$(PWD) clean
	rm -r -f *.o_shipped
