include $(PROJ_ROOT)/Makefile.inc

CFLAGS = $(COMMON_CFLAGS)

BIN_OUT = nkfs_ctl

all: nkfs_ctl
	cp $(BIN_OUT) $(BIN)/$(BIN_OUT)
	rm -rf *.o $(BIN_OUT)

nkfs_ctl: ctl.o main.o
	$(CC) ctl.o main.o $(LIB)/nkfs_crtlib.a -o $(BIN_OUT)

ctl.o: ctl.c
	$(CC) $(CFLAGS) ctl.c

main.o: ctl.c
	$(CC) $(CFLAGS) main.c

clean:
	rm -rf *.o $(BIN_OUT)

