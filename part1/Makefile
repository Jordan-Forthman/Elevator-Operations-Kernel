all: empty part1

empty: empty.c
	gcc -o empty empty.c

part1: part1.c
	gcc -o part1 part1.c

empty.trace: empty
	strace -o empty.trace ./empty

part1.trace: part1
	strace -o part1.trace ./part1

clean:
	rm -f empty part1 empty.trace part1.trace
