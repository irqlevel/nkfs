CC=gcc

CFLAGS_KERNEL=-c -Wall -Werror -Iinclude -mcmodel=kernel
CFLAGS=-c -Wall -Werror -Iinclude

all: error.o char2hex.o
	cp error.o ../objs/error.o
	cp char2hex.o ../objs/char2hex.o
	cp error.o ../objs/error_km.o
	cp char2hex.o ../objs/char2hex_km.o

char2hex_km.o: char2hex.c
	$(CC) $(CFLAGS_KERNEL) char2hex.c

error_km.o: error.c
	$(CC) $(CFLAGS_KERNEL) error.c

char2hex.o: char2hex.c
	$(CC) $(CFLAGS) char2hex.c

error.o: error.c
	$(CC) $(CFLAGS) error.c


clean:
	rm -rf *.o
