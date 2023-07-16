BUILD_DIR = /lib/modules/6.4.2-1-MANJARO-ARM/build
PWD = $(shell pwd)

.PHONY: modules install clean

obj-m += panel-toshiba-tc358775.o

modules:
	$(MAKE) -C $(BUILD_DIR) M=$(PWD) modules

install:
	$(MAKE) -C $(BUILD_DIR) M=$(PWD) modules_install

clean:
	rm -rf *modules.order* *Module.symvers* *panel-toshiba-tc358775.ko* *panel-toshiba-tc358775.mod* *panel-toshiba-tc358775.o*

