CC=gcc
CFLAGS=-c -Wall -Werror -I$(PROJ_ROOT)

all: ds_client
	cp ds_client $(PROJ_ROOT)/bin/ds_client

ds_client: ds_client.o
	$(CC) ds_client.o $(PROJ_ROOT)/libs/ucrt.a $(PROJ_ROOT)/libs/crtlib.a -o ds_client

ds_client.o: ds_client.c
	$(CC) $(CFLAGS) ds_client.c

clean:
	rm -rf *.o ds_client


