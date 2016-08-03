include $(PROJECT_ROOT)/Makefile.inc

CRT_MOD = nkfs_crt
CRT_MOD_KO = $(CRT_MOD).ko

obj-m = $(CRT_MOD).o

ccflags-y := -I$(src) $(PROJECT_CFLAGS) $(PROJECT_DEBUG_CFLAGS)	\
		-D __KERNEL__					\
		-D __MALLOC_CHECKER__				\
		-D __MALLOC_CHECKER_STACK_TRACE__		\
		-D __MALLOC_CHECKER_FILL_CC__			\
		-D __MALLOC_CHECKER_DELAY_FREE__		\
		-D __PAGE_CHECKER__				\
		-D __PAGE_CHECKER_STACK_TRACE__			\
		-D __PAGE_CHECKER_FILL_CC__			\

$(CRT_MOD)-y = page_alloc.o malloc.o page_checker.o		\
		malloc_checker.o vfile.o crt.o			\
		../csum.o ../xxhash.o ../nk8.o ../random.o	\
		../sha256.o ../char2hex.o ../error.o		\
		../obj_id.o ../net_pkt.o

all:
	$(MAKE) -C $(KERNEL_BUILD_PATH) M=$(PROJECT_ROOT)/crt/kernel modules
	cp $(CRT_MOD_KO) $(PROJECT_ROOT)/bin/$(CRT_MOD_KO)
clean:
	$(MAKE) -C $(KERNEL_BUILD_PATH) M=$(PROJECT_ROOT)/crt/kernel clean
