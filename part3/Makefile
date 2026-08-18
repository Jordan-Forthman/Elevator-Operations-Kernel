obj-m += src/elevator.o

# Build against the running kernel unless told otherwise, e.g.
#   make KDIR=/lib/modules/6.8.0-138-generic/build
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# SYSCALLS=1 hooks the custom syscalls 548/549/550 and requires a kernel
# patched per docs/kernel-syscalls.md. The default build is self-contained and
# loads on a stock kernel, driven through /proc/elevator_ctl.
ifeq ($(SYSCALLS),1)
ccflags-y += -DELEVATOR_USE_SYSCALLS
endif

all:
	$(MAKE) -C $(KDIR) M=$(PWD) SYSCALLS=$(SYSCALLS) modules

tools: tools/producer tools/consumer

tools/producer: tools/producer.c
	$(CC) -Wall -O2 -o $@ $<

tools/consumer: tools/consumer.c
	$(CC) -Wall -O2 -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f tools/producer tools/consumer

.PHONY: all tools clean
