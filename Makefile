# Top-level build. Each part can also be built on its own.
#
#   make                 build everything against the running kernel
#   make KDIR=<path>     build the modules against a specific kernel tree
#   make SYSCALLS=1      build part3 to hook syscalls 548-550 (patched kernel)
#   make clean           remove all build output

KDIR     ?= /lib/modules/$(shell uname -r)/build
SYSCALLS ?=

all:
	$(MAKE) -C part1
	$(MAKE) -C part2 KDIR=$(KDIR)
	$(MAKE) -C part3 KDIR=$(KDIR) SYSCALLS=$(SYSCALLS)
	$(MAKE) -C part3 tools

clean:
	$(MAKE) -C part1 clean
	$(MAKE) -C part2 KDIR=$(KDIR) clean
	$(MAKE) -C part3 KDIR=$(KDIR) clean

.PHONY: all clean
