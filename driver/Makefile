obj-m = ds.o

ccflags-y := -I$(src)/include -I$(src)/inc -g3 -Wall -Werror

ds-y := ds/module.o ds/dev_io.o ds/dev.o ds/net.o ds/random.o ds/file.o ds/image.o ds/obj_id.o lib/klog.o lib/ksocket.o

all:
	make -C $(DS_KERNEL_PATH) M=$(PWD) modules
	cp ds.ko ../bin/ds.ko
clean:
	make -C $(DS_KERNEL_PATH) M=$(PWD) clean
	rm -r -f ../bin/ds.ko
