obj-m += awsmgmt.o
awsmgmt-objs := \
	mgmt-core.o \
	mgmt-thread.o \
	mgmt-firewall.o \
	mgmt-bit.o \
	mgmt-cw.o \
	mgmt-sysfs.o 

CONFIG_MODULE_SIG=n
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

PWD       := $(shell pwd)
ROOT := $(dir $(M))
XILINXINCLUDE := -I$(ROOT)/../include

GCCVERSION := $(shell gcc -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/')

GCC49 := $(shell expr $(GCCVERSION) \>= 40900)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

install: all
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install
	depmod -a
	install -m 644 10-awsmgmt.rules /etc/udev/rules.d
	-rmmod -s awsmgmt || true
	-modprobe awsmgmt

clean:
	rm -rf *.o *.o.d *~ core .depend .*.cmd *.ko *.ko.unsigned *.mod.c .tmp_versions *.symvers .#* *.save *.bak Modules.* modules.order Module.markers *.bin

CFLAGS_mgmt-bit.o := $(XILINXINCLUDE)
CFLAGS_mgmt-core.o := $(XILINXINCLUDE)
CFLAGS_mgmt-thread.o := $(XILINXINCLUDE)
CFLAGS_mgmt-firewall.o := $(XILINXINCLUDE)
CFLAGS_mgmt-cw.o := $(XILINXINCLUDE)
CFLAGS_mgmt-sysfs.o := $(XILINXINCLUDE)
