#
# Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
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
export module_path ?= $(PWD)
include $(module_path)/../lib/Makefile.in

# Base on Linux Documentation/kbuild/modules.txt.
# $(src) provides the absolute path by pointing to the directory
# where the currently executing kbuild file is located.
ifneq ($(src),)
# building modules
ifneq ($(wildcard $(src)/../../common),)
# dpms dir
common_dir = ../../common
else
# build from source code
common_dir = ../../../../../common/drv
endif

else
# clean
common_dir = ../../../../../common/drv
ifeq ($(wildcard ../../../../../common/drv),)
common_dir = ../../common
endif

endif

drv_common-y   := $(common_dir)/xrt_xclbin.o

xclmgmt-y := \
	../xocl_subdev.o \
	../xocl_ctx.o \
	../xocl_thread.o \
	../xocl_fdt.o \
	../xocl_xclbin.o \
	../xocl_debug.o \
	../subdev/sysmon.o \
	../subdev/feature_rom.o \
	../subdev/microblaze.o \
	../subdev/ps.o \
	../subdev/firewall.o \
	../subdev/xvc.o \
	../subdev/xiic.o \
	../subdev/mailbox.o \
	../subdev/icap.o \
	../subdev/clock_wiz.o \
	../subdev/clock_counter.o \
	../subdev/mig.o \
	../subdev/xmc.o \
	../subdev/xmc_u2.o \
	../subdev/dna.o \
	../subdev/fmgr.o \
	../subdev/mgmt_msix.o \
	../subdev/flash.o \
	../subdev/iores.o \
	../subdev/axigate.o \
	../subdev/ospi_versal.o \
	../subdev/ert.o \
	../subdev/ddr_srsr.o \
	../subdev/memory_hbm.o \
	../subdev/ulite.o \
	../subdev/calib_storage.o \
	../subdev/pmc.o \
	../subdev/xgq_vmr.o \
	../subdev/icap_cntrl.o \
	../subdev/version_ctrl.o \
	../subdev/pcie_firewall.o \
	../subdev/hwmon_sdm.o \
	../subdev/nifd.o \
	$(drv_common-y) \
	mgmt-core.o \
	mgmt-cw.o \
	mgmt-utils.o \
	mgmt-bifurcation-reset.o \
	mgmt-ioctl.o \
	mgmt-sysfs.o

xclmgmt-y += $(libfdt-y)

CONFIG_MODULE_SIG=n
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

PWD	:= $(shell pwd)
ROOT	:= $(dir $(M))
XILINXINCLUDE := -I$(ROOT)/../include -I$(ROOT)/../../../../include -I$(ROOT)/../../../../common/drv/include

ccflags-y += $(XILINXINCLUDE) -DPF=MGMTPF
ifeq ($(DEBUG),1)
ccflags-y += -DDEBUG
endif

ifeq ($(SYMBOL),1)
	ccflags-y += -g
endif

# We can only rely on CONFIG_FPGA to check whether system has fpga-mgr module support or not.
# But it is not true in some Kernels where this config is enabled but fpga-mgr module is not available
# which is causing xclmgmt driver loading to fail. So commenting this as fpga-mgr is not used anywhere.
# ccflags-$(CONFIG_FPGA) += -DENABLE_FPGA_MGR_SUPPORT
ccflags-y += $(ccflags-m)

all:
	echo $(PWD)
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

install: all
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install
	depmod -a
	install -m 644 99-xclmgmt.rules /etc/udev/rules.d
	-rmmod -s xclmgmt || true
	-modprobe xclmgmt

clean:
	rm -rf *.o *.o.d *.o.cmd *~ core .depend .*.cmd *.ko *.ko.unsigned \
	*.mod.c ../subdev/*.o ../subdev/.*.o.cmd ../*.o ../.*.o.cmd 	\
	../lib/libfdt/*.o ../lib/libfdt/.*.o.cmd ./.cache.mk		\
	.tmp_versions *.symvers modules.order
