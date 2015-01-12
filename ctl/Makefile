include $(PROJ_ROOT)/Makefile.inc

CFLAGS = $(COMMON_CFLAGS)

BIN_OUT = ds_ctl

all: ds_ctl
	cp $(BIN_OUT) $(BIN)/$(BIN_OUT)

ds_ctl: ctl.o main.o test.o
	$(CC) ctl.o main.o test.o $(LIB)/crtlib.a -o ds_ctl

ctl.o: ctl.c
	$(CC) $(CFLAGS) ctl.c

main.o: ctl.c
	$(CC) $(CFLAGS) main.c

test.o: test.c
	$(CC) $(CFLAGS) test.c

clean:
	rm -rf *.o $(BIN_OUT)

