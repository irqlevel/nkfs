CC=gcc
AR=ar

CFLAGS=-c -Wall -Werror -I$(PROJ_ROOT)

LIB_OUT = ucrt.a

all:
	rm -rf *.o *.a
	$(CC) $(CFLAGS) *.c
	$(AR) -cvq $(LIB_OUT) *.o
	cp $(LIB_OUT) $(PROJ_ROOT)/libs/$(LIB_OUT)

clean:
	rm -rf *.o *.a
