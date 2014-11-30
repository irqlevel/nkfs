CC=gcc
AR=ar
ARCH_BITS=$(shell getconf LONG_BIT)
CFLAGS=-c -Wall -Werror -I$(PROJ_ROOT)
CFLAGS_KERNEL=$(CFLAGS) -D __KERNEL__
ifeq ($(ARCH_BITS), 64)
CFLAGS_KERNEL=$(CFLAGS_KERNEL) -mcmodel=kernel
endif

CFLAGS=-c -Wall -Werror -I$(PROJ_ROOT)

LIB_OUT = crtlib.a
LIB_KOUT = crtlib_km.a

LIBS_SRC = sha256.c char2hex.c error.c obj_id.c

OBJS = $(LIBS_OBJ) $(LIBS_KOBJ)

all:
	rm -rf *.o *.a
	$(CC) $(CFLAGS_KERNEL) *.c
	$(AR) -cvq $(LIB_KOUT) *.o
	cp $(LIB_KOUT) ../libs/$(LIB_KOUT)
	rm -rf *.o *.a
	$(CC) $(CFLAGS) *.c
	$(AR) -cvq $(LIB_OUT) *.o
	cp $(LIB_OUT) ../libs/$(LIB_OUT)

clean:
	rm -rf *.o *.a
