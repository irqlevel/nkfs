CC=gcc
CFLAGS=-c -Wall -Werror -Iinclude -mcmodel=kernel

all: error.o char2hex.o
	cp error.o ../objs/error.o
	cp char2hex.o ../objs/char2hex.o

char2hex.o: char2hex.c
	$(CC) $(CFLAGS) char2hex.c

error.o: error.c
	$(CC) $(CFLAGS) error.c

clean:
	rm -rf *.o
