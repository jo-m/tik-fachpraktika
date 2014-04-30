obj-m	:= kernel_morse.o

KDIR	:= /lib/modules/$(shell uname -r)/build

all: module

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
run: module
	rm /dev/morse
	mknod /dev/morse c 251 100
	rmmod kernel_morse || true
	insmod kernel_morse.ko
	logger -- --------------------------
	echo HALLO > /dev/morse
