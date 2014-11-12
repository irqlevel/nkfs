obj-m = ds.o

ccflags-y := -I$(src)/inc -I$(PROJ_ROOT) -g3 -Wall -Werror

ds-y := module.o dev_io.o dev.o net.o random.o file.o \
	image.o obj_id.o \
	lib/klog.o lib/ksocket.o lib/sha256.o \
	error.o_shipped char2hex.o_shipped

all:
	cp $(PROJ_ROOT)/objs/error_km.o error.o_shipped
	cp $(PROJ_ROOT)/objs/char2hex_km.o char2hex.o_shipped
	make -C $(DS_KERNEL_PATH) M=$(PWD) modules
	cp ds.ko ../bin/ds.ko
clean:
	make -C $(DS_KERNEL_PATH) M=$(PWD) clean

