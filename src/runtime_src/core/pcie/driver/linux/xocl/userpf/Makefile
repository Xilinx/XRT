#
# Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
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
	../xocl_fdt.o \
	../subdev/xdma.o \
	../subdev/qdma.o \
	../subdev/feature_rom.o \
	../subdev/mb_scheduler.o \
	../subdev/mailbox.o \
	../subdev/xvc.o \
	../subdev/icap.o \
	../subdev/clock.o \
	../subdev/xmc.o \
	../subdev/firewall.o \
	../subdev/mig.o \
	../subdev/dna.o \
	../subdev/iores.o \
	../subdev/mailbox_versal.o\
	../subdev/aim.o\
	../subdev/am.o\
	../subdev/asm.o\
	../subdev/trace_fifo_lite.o\
	../subdev/trace_fifo_full.o\
	../subdev/trace_funnel.o\
	../subdev/trace_s2mm.o\
	../subdev/memory_hbm.o \
	$(xocl_lib-y)	\
	xocl_drv.o	\
	xocl_bo.o	\
	xocl_drm.o	\
	xocl_ioctl.o	\
	xocl_sysfs.o

xocl-y += $(libfdt-y)

CONFIG_MODULE_SIG=n
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

PWD	:= $(shell pwd)
ROOT	:= $(dir $(M))
XILINXINCLUDE := -I$(ROOT)/../include -I$(ROOT)/../../../../include -I$(ROOT)/../../../../common/drv/

ccflags-y += $(XILINXINCLUDE) -DPF=USERPF -D__XRT__
ifeq ($(DEBUG),1)
ccflags-y += -DDEBUG
endif

ifeq ($(SYMBOL),1)
ccflags-y += -g
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
	../lib/libqdma/*.o ../lib/libqdma/.*.o.cmd 			\
	../lib/libqdma/*.o.ur-safe ../subdev/*.o.ur-safe ./.cache.mk	\
	.tmp_versions *.symvers modules.order
