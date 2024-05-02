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

include $(src)/../lib/Makefile
common_dir = ../../common

drv_common-y   := $(common_dir)/xrt_xclbin.o

obj-$(CONFIG_XCLMGMT) += \
	../subdev/sysmon.o \
	../subdev/microblaze.o \
	../subdev/ps.o \
	../subdev/nifd.o \
	../subdev/xiic.o \
	../subdev/fmgr.o \
	../subdev/mgmt_msix.o \
	../subdev/flash.o \
	../subdev/axigate.o \
	../subdev/ospi_versal.o \
	../subdev/ert.o \
	../subdev/ulite.o \
	../subdev/calib_storage.o \
	../subdev/pmc.o \
	../subdev/xgq_vmr.o \
	../subdev/icap_cntrl.o \
	../subdev/pcie_firewall.o

obj-$(CONFIG_XCLMGMT) += mgmt-core.o
obj-$(CONFIG_XCLMGMT) += mgmt-cw.o
obj-$(CONFIG_XCLMGMT) += mgmt-utils.o
obj-$(CONFIG_XCLMGMT) += mgmt-bifurcation-reset.o
obj-$(CONFIG_XCLMGMT) += mgmt-ioctl.o
obj-$(CONFIG_XCLMGMT) += mgmt-sysfs.o

XILINXINCLUDE  := -I. -I$(src)/../../include -I$(src)/../include -I$(src)/../ -I$(src)/../lib/libqdma -I$(src)/../lib/libqdma/QDMA/linux-kerenl/driver/libqdma/qdma_access

ccflags-y += $(XILINXINCLUDE) -DPF=MGMTPF
