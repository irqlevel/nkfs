CC=gcc
AR=ar
CFLAGS=-c -Wall -Werror -I$(PROJ_ROOT)
LIB_OUT= ds_cl.a

all: ds_client 
	 cp ds_client $(PROJ_ROOT)/bin/ds_client
	 cp lib/$(LIB_OUT) $(PROJ_ROOT)/libs/$(LIB_OUT)

ds_client: ds_client.o ds_cl.o
	$(CC) ds_client.o lib/ds_cl.a $(PROJ_ROOT)/libs/ds_cl.a $(PROJ_ROOT)/libs/ucrt.a $(PROJ_ROOT)/libs/crtlib.a -o ds_client

ds_client.o: ds_client.c
	$(CC) $(CFLAGS) ds_client.c
	
ds_cl.o:
	$(CC) $(CFLAGS) lib/ds_cl.c
	$(AR) -cvq lib/$(LIB_OUT) lib/ds_cl.o
clean:
	rm -rf *.o ds_client lib/*.a lib/*.o


