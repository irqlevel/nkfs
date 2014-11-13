CC=gcc
CFLAGS=-c -Wall -Werror -I$(PROJ_ROOT)

all: ds_ctl
	cp ds_ctl $(PROJ_ROOT)/bin/ds_ctl

ds_ctl: ds_ctl.o
	$(CC) ds_ctl.o -o ds_ctl

ds_ctl.o: ds_ctl.c
	$(CC) $(CFLAGS) ds_ctl.c

clean:
	rm -rf *.o ds_ctl

