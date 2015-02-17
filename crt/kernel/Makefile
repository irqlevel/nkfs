include $(PROJ_ROOT)/Makefile.inc

CRT_MOD = nkfs_crt
CRT_MOD_KO = $(CRT_MOD).ko

obj-m = $(CRT_MOD).o

ccflags-y := -I$(src) -g3 -D __KERNEL__ $(COMMON_CFLAGS)

$(CRT_MOD)-y = klog.o vfile.o crt.o ../csum.o ../xxhash.o ../nk8.o ../random.o ../sha256.o ../char2hex.o ../error.o ../obj_id.o ../net_pkt.o

all:
	make -C $(NKFS_KERNEL_PATH) M=$(PWD) modules
	cp $(CRT_MOD_KO) $(PROJ_ROOT)/bin/$(CRT_MOD_KO)
clean:
	make -C $(NKFS_KERNEL_PATH) M=$(PWD) clean
