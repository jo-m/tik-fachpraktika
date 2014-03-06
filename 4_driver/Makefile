obj-m := kernel_morse.o

KDIR  := /lib/modules/$(shell uname -r)/build

all: module

module:
  $(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
  $(MAKE) -C $(KDIR) M=$(PWD) clean
