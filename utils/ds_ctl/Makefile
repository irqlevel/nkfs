include $(PROJ_ROOT)/Makefile.inc

CFLAGS = $(COMMON_CFLAGS)

all: ds_ctl
	cp ds_ctl $(PROJ_ROOT)/bin/ds_ctl
	rm -rf *.o ds_ctl

ds_ctl: ds_ctl.o
	$(CC) ds_ctl.o $(PROJ_ROOT)/libs/ucrt.a $(PROJ_ROOT)/libs/crtlib.a -o ds_ctl

ds_ctl.o: ds_ctl.c
	$(CC) $(CFLAGS) ds_ctl.c

clean:
	rm -rf *.o ds_ctl

