CC=gcc
CFLAGS=-I0

test.o: flash_led.c
	$(CC) -c -o test.o flash_led.c

test: test.o
	$(CC) -o test test.o

.PHONY: clean
clean:
	rm -f test.o test