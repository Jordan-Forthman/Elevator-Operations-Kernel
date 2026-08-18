# Top-level build. Each component can also be built on its own.
#
#   make                 build everything against the running kernel
#   make KDIR=<path>     build the modules against a specific kernel tree
#   make SYSCALLS=1      build the elevator to hook syscalls 548-550
#   make clean           remove all build output

KDIR     ?= /lib/modules/$(shell uname -r)/build
SYSCALLS ?=

all:
	$(MAKE) -C syscall-tracing
	$(MAKE) -C timer-module KDIR=$(KDIR)
	$(MAKE) -C elevator-module KDIR=$(KDIR) SYSCALLS=$(SYSCALLS)
	$(MAKE) -C elevator-module tools

clean:
	$(MAKE) -C syscall-tracing clean
	$(MAKE) -C timer-module KDIR=$(KDIR) clean
	$(MAKE) -C elevator-module KDIR=$(KDIR) clean

.PHONY: all clean
