CC     ?= gcc
CFLAGS ?= -Wall -O2

# the verify target uses process substitution
SHELL := /bin/bash

PROGS  := empty part1
TRACES := empty.trace part1.trace

all: $(PROGS)

empty: empty.c
	$(CC) $(CFLAGS) -o $@ $<

part1: part1.c
	$(CC) $(CFLAGS) -o $@ $<

# Regenerate the strace logs. part1 reads a byte from stdin, so feed it from
# /dev/null to keep this target non-interactive.
trace: $(TRACES)

empty.trace: empty
	strace -o $@ ./empty < /dev/null

part1.trace: part1
	strace -o $@ ./part1 < /dev/null

# part1 should make exactly five more syscalls than the empty baseline.
verify: $(TRACES)
	@echo "empty.trace: $$(wc -l < empty.trace) syscalls"
	@echo "part1.trace: $$(wc -l < part1.trace) syscalls"
	@echo "added:       $$(( $$(wc -l < part1.trace) - $$(wc -l < empty.trace) ))"
	@echo "the added calls are:"
	@diff <(sed 's/(.*//' empty.trace | sort) \
	      <(sed 's/(.*//' part1.trace | sort) | sed -n 's/^> /  /p'

clean:
	rm -f $(PROGS) $(TRACES)

.PHONY: all trace verify clean
