#
# Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
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

#obj-y += xocl.o
ccflags-y := -Iinclude/drm

export module_path ?= $(PWD)
include $(src)/../lib/Makefile
common_dir = ../../common

drv_common-y   := $(common_dir)/kds_core.o \
		  $(common_dir)/xrt_cu.o \
		  $(common_dir)/cu_hls.o \
		  $(common_dir)/fast_adapter.o \
		  $(common_dir)/xgq_execbuf.o \
		  $(common_dir)/xrt_xclbin.o

obj-$(CONFIG_XOCL) += \
	../xocl_subdev.o \
	../xocl_ctx.o \
	../xocl_thread.o \
	../xocl_fdt.o \
	../xocl_xclbin.o \
	../xocl_debug.o \
	../xocl_xgq.o \
	../subdev/xdma.o \
	../subdev/qdma.o \
	../subdev/feature_rom.o \
	../subdev/mailbox.o \
	../subdev/xvc.o \
	../subdev/icap.o \
	../subdev/clock_wiz.o \
	../subdev/clock_counter.o \
	../subdev/xmc.o \
	../subdev/xmc_u2.o \
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
	../subdev/lapc.o\
	../subdev/spc.o\
	../subdev/ddr_srsr.o\
	../subdev/memory_hbm.o \
	../subdev/address_translator.o \
	../subdev/cu.o \
	../subdev/scu.o \
	../subdev/p2p.o \
	../subdev/intc.o \
	../subdev/m2m.o \
	../subdev/version_ctrl.o \
	../subdev/msix_xdma.o \
	../subdev/ert_user.o \
	../subdev/accel_deadlock_detector.o \
	../subdev/command_queue.o \
	../subdev/cfg_gpio.o \
	../subdev/hwmon_sdm.o \
	../subdev/ert_ctrl.o \
	../subdev/cu_xgq.o \
	$(xocl_lib-y)	\
	$(drv_common-y) \
#	xocl_drv.o	\
#	xocl_errors.o	\
#	xocl_bo.o	\
#	xocl_drm.o	\
#	xocl_ioctl.o	\
#	xocl_kds.o 	\
#	xocl_sysfs.o

obj-$(CONFIG_XOCL) += $(libfdt-y)

obj-$(CONFIG_XOCL) += xocl_drv.o
obj-$(CONFIG_XOCL) += xocl_errors.o
obj-$(CONFIG_XOCL) += xocl_bo.o
obj-$(CONFIG_XOCL) += xocl_drm.o
obj-$(CONFIG_XOCL) += xocl_ioctl.o
obj-$(CONFIG_XOCL) += xocl_kds.o
obj-$(CONFIG_XOCL) += xocl_sysfs.o
obj-$(CONFIG_XOCL) += $(drv_common-y)


XILINXINCLUDE := -I. -I$(src)/../../include -I$(src)/../include -I$(src)/../ -I$(src)/../lib/libqdma -I$(src)../lib/libqdma/QDMA/linux-kernel/driver/libqdma/qdma_access/qdma_soft_access -I$(src)/../lib/libqdma/QDMA/linux-kernel/driver/libqdma/qdma_access

ccflags-y += $(XILINXINCLUDE) -DPF=USERPF -D__XRT__

QDMA_PATH := $(src)../lib/libqdma/QDMA/linux-kernel/driver/libqdma/
ccflags-y += -I$(QDMA_PATH) -I$(QDMA_PATH)/qdma_access/ -I$(QDMA_PATH)/qdma_access/qdma_soft_access/ -I$(QDMA_PATH)/qdma_access/eqdma_soft_access -I$(QDMA_PATH)/qdma_access/qdma_cpm4_access/ -I$(QDMA_PATH)/qdma_access/eqdma_cpm5_access/
