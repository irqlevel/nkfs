include $(PROJ_ROOT)/Makefile.inc

CFLAGS= $(COMMON_CFLAGS) -D__CRT_LIB__

LIB_OUT = crtlib.a

LIB_SRC = crt.c ../random.c ../sha256.c ../char2hex.c ../error.c ../obj_id.c ../net_cmd.c

OBJS = $(LIBS_OBJ) $(LIBS_KOBJ)

all:
	rm -rf *.o *.a
	$(CC) $(CFLAGS) $(LIB_SRC)
	$(AR) -cvq $(LIB_OUT) *.o
	cp $(LIB_OUT) $(PROJ_ROOT)/libs/$(LIB_OUT)

clean:
	rm -rf *.o *.a
