obj-m = ds.o

ccflags-y := -I$(src)/include -I$(src)/inc -g3 -Wall -Werror

ds-y := ds/main.o lib/klog.o lib/ksocket.o

KVER = $(shell uname -r)
all:
	make -C /lib/modules/$(KVER)/build M=$(PWD) modules
	cp ds.ko ../bin/ds.ko
clean:
	make -C /lib/modules/$(KVER)/build M=$(PWD) clean
	rm -r -f ../bin/ds.ko
