include $(PROJ_ROOT)/Makefile.inc

CFLAGS= $(COMMON_CFLAGS) -D__CRT_LIB__

CFLAGS_KERNEL=$(CFLAGS) -D__KERNEL__

ifeq ($(ARCH_BITS), 64)
CFLAGS_KERNEL += -mcmodel=kernel
endif

LIB_OUT = crtlib.a
LIB_KOUT = crtlib_km.a

LIB_SRC = random.c sha256.c char2hex.c error.c obj_id.c net_cmd.c

OBJS = $(LIBS_OBJ) $(LIBS_KOBJ)

all:
	rm -rf *.o *.a
	$(CC) $(CFLAGS_KERNEL) $(LIB_SRC)
	$(AR) -cvq $(LIB_KOUT) *.o
	cp $(LIB_KOUT) ../libs/$(LIB_KOUT)
	rm -rf *.o *.a
	$(CC) $(CFLAGS) $(LIB_SRC)
	$(AR) -cvq $(LIB_OUT) *.o
	cp $(LIB_OUT) ../libs/$(LIB_OUT)

clean:
	rm -rf *.o *.a
