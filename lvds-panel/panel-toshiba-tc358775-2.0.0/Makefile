KERNEL_BUILD_DIR = /lib/modules/$(KERNEL_VER)/build
PWD = $(shell pwd)

obj-m += panel-toshiba-tc358775.o

modules:
	$(MAKE) -C $(KERNEL_BUILD_DIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KERNEL_BUILD_DIR) M=$(PWD) modules_install

clean:
	rm -rf $(shell ls -A | grep -v panel-toshiba-tc358775.c | grep -v Makefile)

.PHONY: modules install clean
