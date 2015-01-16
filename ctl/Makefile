include $(PROJ_ROOT)/Makefile.inc

CFLAGS = $(COMMON_CFLAGS)

BIN_OUT = ds_ctl

all: ds_ctl
	cp $(BIN_OUT) $(BIN)/$(BIN_OUT)
	rm -rf *.o $(BIN_OUT)

ds_ctl: ctl.o main.o
	$(CC) ctl.o main.o $(LIB)/crtlib.a -o ds_ctl

ctl.o: ctl.c
	$(CC) $(CFLAGS) ctl.c

main.o: ctl.c
	$(CC) $(CFLAGS) main.c

clean:
	rm -rf *.o $(BIN_OUT)

