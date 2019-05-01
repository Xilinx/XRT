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

obj-m	+= xocl.o
ccflags-y := -Iinclude/drm

include $(PWD)/../lib/Makefile.in
xocl-y := \
	../xocl_subdev.o \
	../xocl_ctx.o \
	../xocl_thread.o \
	../subdev/xdma.o \
	../subdev/qdma.o \
	../subdev/feature_rom.o \
	../subdev/mb_scheduler.o \
	../subdev/mailbox.o \
	../subdev/xvc.o \
	../subdev/icap.o \
	../subdev/xmc.o \
	$(xocl_lib-y)	\
	xocl_drv.o	\
	xocl_bo.o	\
	xocl_drm.o	\
	xocl_ioctl.o	\
	xocl_sysfs.o


CONFIG_MODULE_SIG=n
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

PWD	:= $(shell pwd)

ROOT	:= $(dir $(M))
XILINXINCLUDE := -I$(ROOT)/../../include -I$(ROOT)/../../../include -I${ROOT}/../../xrt/user_gem -I${ROOT}/../../../../

ccflags-y += $(XILINXINCLUDE) -DSUBDEV_SUFFIX=USER_SUFFIX -D__XRT__
ifeq ($(DEBUG),1)
ccflags-y += -DDEBUG
endif

all:
	echo $(PWD)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

install: all
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install
	depmod -a
	install -m 644 10-xocl.rules /etc/udev/rules.d
	-rmmod -s xocl || true
	-rmmod -s xdma || true
	-modprobe xocl

clean:
	rm -rf *.o *.o.d *~ core .depend .*.cmd *.ko *.ko.unsigned *.mod.c \
	../subdev/*.o ../subdev/.*.o.cmd ../lib/*.o ../lib/.*.o.cmd	\
	../lib/libqdma/*.o ../lib/libqdma/.*.o.cmd 	\
	../lib/libqdma/*.o.ur-safe \
	.tmp_versions *.symvers modules.order
	#* *.save *.bak Modules.* modules.order Module.markers *.bin
