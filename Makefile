all: uxnbruteforce

uxnbruteforce: uxn-fast.c uxn.h main.c
	gcc uxn-fast.c main.c -o uxnbruteforce -ansi -Wall -g -O3

.PHONY: run clean

run: all
	./uxnbruteforce

clean:
	rm -f uxnbruteforce *~
