include $(PROJECT_ROOT)/Makefile.inc

CFLAGS= $(PROJECT_CFLAGS) $(PROJECT_DEBUG_CFLAGS) -D__CRT_LIB__

LIB_OUT = nkfs_crtlib.a

LIB_SRC = crt.c ../csum.c ../xxhash.c ../random.c ../sha256.c ../char2hex.c ../error.c ../obj_id.c ../net_pkt.c

OBJS = $(LIBS_OBJ) $(LIBS_KOBJ)

all:
	rm -rf *.o *.a
	$(CC) -c $(CFLAGS) $(LIB_SRC)
	$(AR) -cvq $(LIB_OUT) *.o
	cp $(LIB_OUT) $(LIB)/$(LIB_OUT)
	rm -rf $(LIB_OUT)
clean:
	rm -rf *.o *.a
