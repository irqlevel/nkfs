include $(PROJ_ROOT)/Makefile.inc

CFLAGS = $(COMMON_CFLAGS)

all: ds_ctl
	cp ds_ctl $(PROJ_ROOT)/bin/ds_ctl
	rm -rf *.o ds_ctl

ds_ctl: ctl.o main.o test.o
	$(CC) ctl.o main.o test.o $(PROJ_ROOT)/libs/ucrt.a $(PROJ_ROOT)/libs/crtlib.a -o ds_ctl

ctl.o: ctl.c
	$(CC) $(CFLAGS) ctl.c

main.o: ctl.c
	$(CC) $(CFLAGS) main.c

test.o: test.c
	$(CC) $(CFLAGS) test.c

clean:
	rm -rf *.o ds_ctl

