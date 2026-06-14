CC = clang
CFLAGS=-Wall -Wextra -Werror -pedantic

all: hello

hello: hello.c
	$(CC) $(CFLAGS) -o hello hello.c

clean:
	rm -f hello *.o