include $(PROJ_ROOT)/Makefile.inc

CRT_MOD = nkfs_crt
CRT_MOD_KO = $(CRT_MOD).ko

obj-m = $(CRT_MOD).o

ccflags-y := -I$(src) -g3 -D __KERNEL__ $(COMMON_CFLAGS)	\
		-D __MALLOC_CHECKER__				\
		-D __MALLOC_CHECKER_STACK_TRACE__		\
		-D __MALLOC_CHECKER_FILL_CC__			\
		-D __MALLOC_CHECKER_DELAY_FREE__		\


$(CRT_MOD)-y = malloc_checker.o klog.o vfile.o crt.o		\
		../csum.o ../xxhash.o ../nk8.o ../random.o	\
		../sha256.o ../char2hex.o ../error.o		\
		../obj_id.o ../net_pkt.o

all:
	$(MAKE) -C $(NKFS_KERNEL_PATH) M=$(PROJ_ROOT)/crt/kernel modules
	cp $(CRT_MOD_KO) $(PROJ_ROOT)/bin/$(CRT_MOD_KO)
clean:
	$(MAKE) -C $(NKFS_KERNEL_PATH) M=$(PROJ_ROOT)/crt/kernel clean
