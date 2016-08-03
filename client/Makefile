include $(PROJECT_ROOT)/Makefile.inc

LIB_OUT=nkfs_client.a
BIN_OUT=nkfs_client
CFLAGS = $(PROJECT_CFLAGS) $(PROJECT_EXTRA_CFLAGS)

all: $(BIN_OUT)
	cp $(BIN_OUT) $(BIN)/$(BIN_OUT)
	cp lib/$(LIB_OUT) $(LIB)/$(LIB_OUT)
	rm -rf *.o $(BIN_OUT) lib/*.a lib/*.o

$(BIN_OUT): client.o main.o
	$(CC) main.o lib/$(LIB_OUT) $(LIB)/nkfs_crtlib.a -o $(BIN_OUT)

main.o: main.c
	$(CC) -c $(CFLAGS) main.c

client.o:
	$(CC) -c $(CFLAGS) lib/client.c
	$(AR) -cvq lib/$(LIB_OUT) client.o

clean:
	rm -rf *.o $(BIN_OUT) lib/*.a lib/*.o

