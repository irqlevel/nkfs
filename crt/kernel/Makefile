include $(PROJ_ROOT)/Makefile.inc

obj-m = ds_crt.o

ccflags-y := -I$(src) -g3 -D __KERNEL__ $(COMMON_CFLAGS)

ds_crt-y = klog.o vfile.o crt.o ../nk8.o ../random.o ../sha256.o ../char2hex.o ../error.o ../obj_id.o ../net_pkt.o

ifndef DS_KERNEL_PATH
DS_KERNEL_PATH=/lib/modules/$(shell uname -r)/build
endif

all:
	make -C $(DS_KERNEL_PATH) M=$(PWD) modules
	cp ds_crt.ko $(PROJ_ROOT)/bin/ds_crt.ko
clean:
	make -C $(DS_KERNEL_PATH) M=$(PWD) clean
