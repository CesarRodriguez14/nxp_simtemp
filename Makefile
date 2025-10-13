obj-m += nxp_simtemp_drv.o

nxp_simtemp_drv-objs := nxp_simtemp.o gaussian_random.o

KDIR = /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(shell pwd) modules

clear:
	make -C $(KDIR) M=$(shell pwd) clean
