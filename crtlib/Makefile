CC=gcc
AR=ar

CFLAGS_KERNEL=-c -Wall -Werror -Iinclude -I$(PROJ_ROOT) -mcmodel=kernel -D __KERNEL__
CFLAGS=-c -Wall -Werror -Iinclude -I$(PROJ_ROOT)

LIB_OUT = crtlib.a
LIB_KOUT = crtlib_km.a

LIBS_SRC = char2hex.c error.c obj_id.c

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
