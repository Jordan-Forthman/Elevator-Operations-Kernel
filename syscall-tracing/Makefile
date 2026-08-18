CC     ?= gcc
CFLAGS ?= -Wall -O2

# the verify target uses process substitution
SHELL := /bin/bash

PROGS  := empty traced
TRACES := empty.trace traced.trace

all: $(PROGS)

empty: empty.c
	$(CC) $(CFLAGS) -o $@ $<

traced: traced.c
	$(CC) $(CFLAGS) -o $@ $<

# Regenerate the strace logs. traced reads a byte from stdin, so feed it from
# /dev/null to keep this target non-interactive.
trace: $(TRACES)

empty.trace: empty
	strace -o $@ ./empty < /dev/null

traced.trace: traced
	strace -o $@ ./traced < /dev/null

# traced should make exactly five more syscalls than the empty baseline.
verify: $(TRACES)
	@echo "empty.trace:  $$(wc -l < empty.trace) syscalls"
	@echo "traced.trace: $$(wc -l < traced.trace) syscalls"
	@echo "added:        $$(( $$(wc -l < traced.trace) - $$(wc -l < empty.trace) ))"
	@echo "the added calls are:"
	@diff <(sed 's/(.*//' empty.trace | sort) \
	      <(sed 's/(.*//' traced.trace | sort) | sed -n 's/^> /  /p'

clean:
	rm -f $(PROGS) $(TRACES)

.PHONY: all trace verify clean
