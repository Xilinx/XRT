#
# Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
#
# Authors:
#
# This software is licensed under the terms of the GNU General Public
# License version 2, as published by the Free Software Foundation, and
# may be copied, distributed, and modified under those terms.
#
# This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

obj-m	+= xclmgmt.o

xclmgmt-y := \
	../xocl_subdev.o \
	../xocl_ctx.o \
	../xocl_thread.o \
	../subdev/sysmon.o \
	../subdev/feature_rom.o \
	../subdev/microblaze.o \
	../subdev/firewall.o \
	../subdev/xvc.o \
	../subdev/nifd.o \
	../subdev/xiic.o \
	../subdev/mailbox.o \
	../subdev/icap.o \
	../subdev/mig.o \
	../subdev/xmc.o \
	../subdev/dna.o \
	../subdev/fmgr.o \
	mgmt-core.o \
	mgmt-cw.o \
	mgmt-utils.o \
	mgmt-ioctl.o \
	mgmt-sysfs.o


CONFIG_MODULE_SIG=n
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

PWD	:= $(shell pwd)

ROOT	:= $(dir $(M))
XILINXINCLUDE := -I$(ROOT)/../../include -I$(ROOT)/../../../include

ccflags-y += $(XILINXINCLUDE) -DSUBDEV_SUFFIX=MGMT_SUFFIX
ifeq ($(DEBUG),1)
ccflags-y += -DDEBUG
endif

all:
	echo $(PWD)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

install: all
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install
	depmod -a
	install -m 644 10-xclmgmt.rules /etc/udev/rules.d
	-rmmod -s xclmgmt || true
	-modprobe xclmgmt

clean:
	rm -rf *.o *.o.d *.o.cmd *~ core .depend .*.cmd *.ko *.ko.unsigned \
	*.mod.c ../subdev/*.o ../subdev/.*.o.cmd ../*.o ../.*.o.cmd 	\
	.tmp_versions *.symvers modules.order
	#* *.save *.bak Modules.* modules.order Module.markers *.bin
