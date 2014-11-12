CC=gcc

CFLAGS_KERNEL=-c -Wall -Werror -Iinclude -mcmodel=kernel
CFLAGS=-c -Wall -Werror -Iinclude

CHAR2HEX_OBJ = char2hex.o
CHAR2HEX_KOBJ = char2hex_km.o

ERROR_OBJ = error.o
ERROR_KOBJ = error_km.o

OBJS = $(CHAR2HEX_OBJ) $(CHAR2HEX_KOBJ) $(ERROR_OBJ) $(ERROR_KOBJ)

all:	$(OBJS)
	$(foreach ofile, $(OBJS), cp $(ofile) ../objs/$(ofile);)

$(CHAR2HEX_KOBJ): char2hex.c
	$(CC) $(CFLAGS_KERNEL) char2hex.c -o $(CHAR2HEX_KOBJ)

$(ERROR_KOBJ): error.c
	$(CC) $(CFLAGS_KERNEL) error.c -o $(ERROR_KOBJ)

$(CHAR2HEX_OBJ): char2hex.c
	$(CC) $(CFLAGS) char2hex.c -o $(CHAR2HEX_OBJ)

$(ERROR_OBJ): error.c
	$(CC) $(CFLAGS) error.c -o $(ERROR_OBJ)


clean:
	rm -rf *.o
