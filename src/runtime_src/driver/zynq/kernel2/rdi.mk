include $(RDI_MAKEROOT)/top.mk
include $(RDI_MAKEROOT)/platform.mk

SDACCEL_RUNTIME_DIR := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src

ZYNQ_KERNEL_DRV_SRC := \
 driver/zynq/kernel2/drm/Makefile \
 driver/zynq/kernel2/drm/README \
 driver/zynq/kernel2/drm/zocl/xlnk_dts_fragment_mpsoc.dts \
 driver/zynq/kernel2/drm/zocl/xlnk_dts_fragment_mpsoc_smmu.dts \
 driver/zynq/kernel2/drm/zocl/xlnk_dts_fragment_zynq.dts \
 driver/zynq/kernel2/drm/zocl/sched_exec.c \
 driver/zynq/kernel2/drm/zocl/sched_exec.h \
 driver/zynq/kernel2/drm/zocl/Makefile \
 driver/zynq/kernel2/drm/zocl/xclbin.h \
 driver/zynq/kernel2/drm/zocl/zocl_bo.c \
 driver/zynq/kernel2/drm/zocl/zocl_drv.c \
 driver/zynq/kernel2/drm/zocl/zocl_drv.h \
 driver/zynq/kernel2/drm/zocl/zocl_sysfs.c \
 driver/zynq/kernel2/drm/zocl/zocl_ioctl.c \
 driver/zynq/kernel2/drm/zocl/zocl_ioctl.h \
 driver/zynq/include/zynq_ioctl.h

ZYNQ_DRV_ARCHIVE_NAME := zocl.zip
ZIP_SRC := $(addprefix $(SDACCEL_RUNTIME_DIR)/, $(ZYNQ_KERNEL_DRV_SRC))
ZIP_TAR := $(RDI_BUILD_DIR)/$(SSNAME)/$(SSDIR)/$(ZYNQ_DRV_ARCHIVE_NAME)

$(ZIP_TAR) : $(ZIP_SRC) | $(dir $(ZIP_TAR)).rdi
	cd $(SDACCEL_RUNTIME_DIR); $(_ZIP) $@ $(ZYNQ_KERNEL_DRV_SRC)

ifeq ($(RDI_PLATFORM),lnx64)
 RELEASE_DATA_TARGET := release
 RELEASE_DATA_DIR := data/sdaccel/zynq/src
 RELEASE_TYPE := customer
 RELEASE_DATA_EXACT := $(ZIP_TAR)
 include $(RDI_MAKEROOT)/release.mk
endif

include $(RDI_MAKEROOT)/bottom.mk
