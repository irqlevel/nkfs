include $(PROJ_ROOT)/Makefile.inc

LIB_OUT=ds_client.a
BIN_OUT=ds_client
CFLAGS = $(COMMON_CFLAGS)

all: ds_client 
	cp ds_client $(PROJ_ROOT)/bin/ds_client
	cp lib/$(LIB_OUT) $(PROJ_ROOT)/libs/$(LIB_OUT)

ds_client: ds_client.o main.o
	$(CC) main.o lib/ds_client.a $(PROJ_ROOT)/libs/ucrt.a $(PROJ_ROOT)/libs/crtlib.a -o $(BIN_OUT)

main.o: main.c
	$(CC) $(CFLAGS) main.c
	
ds_client.o:
	$(CC) $(CFLAGS) lib/ds_client.c
	$(AR) -cvq lib/$(LIB_OUT) ds_client.o

clean:
	rm -rf *.o ds_client lib/*.a lib/*.o

