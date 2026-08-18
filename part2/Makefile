obj-m += src/my_timer.o

# Build against the running kernel unless told otherwise, e.g.
#   make KDIR=/lib/modules/6.8.0-138-generic/build
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
