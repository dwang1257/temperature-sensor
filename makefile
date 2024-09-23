CC=gcc
CFLAGS=-lpthread -I.

test: lab4.c
	$(CC) -o test lab4.c $(CFLAGS)
.PHONY: clean
clean:
	rm -f test
