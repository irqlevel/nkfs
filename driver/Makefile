obj-m = ds.o

ccflags-y := -I$(src)/inc -I$(PROJ_ROOT) -g3 -Wall -Werror

ds-y := module.o dev_io.o dev.o net.o random.o file.o \
	image.o crt.o \
	lib/klog.o lib/ksocket.o lib/sha256.o \
	crtlib.o_shipped

all:
	cp $(PROJ_ROOT)/libs/crtlib_km.a crtlib.o_shipped
	make -C $(DS_KERNEL_PATH) M=$(PWD) modules
	cp ds.ko ../bin/ds.ko
clean:
	make -C $(DS_KERNEL_PATH) M=$(PWD) clean

