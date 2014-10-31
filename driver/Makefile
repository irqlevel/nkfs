obj-m = ds.o

ccflags-y := -I$(src)/include -I$(src)/inc -g3 -Wall -Werror

ds-y := ds/main.o lib/klog.o lib/ksocket.o

all:
	make -C /mnt/sources/linux-stable M=$(PWD) modules
	cp ds.ko ../bin/ds.ko
clean:
	make -C /mnt/sources/linux-stable M=$(PWD) clean
	rm -r -f ../bin/ds.ko
