CC = clang
CFLAGS=-Wall -Wextra -Werror -pedantic

all: memory

memory: memory.c
	$(CC) $(CFLAGS) -o memory memory.c

clean:
	rm -f memory *.o