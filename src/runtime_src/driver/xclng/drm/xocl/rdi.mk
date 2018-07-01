include $(RDI_MAKEROOT)/top.mk
include $(RDI_MAKEROOT)/platform.mk

SDACCEL_RUNTIME_DIR := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src
include $(SDACCEL_RUNTIME_DIR)/driver/xclng/xclng_release.mk

XOCL_KERNEL_DRV_SRC := \
 driver/xclng/drm/xocl/mgmtpf/mgmt-core.c \
 driver/xclng/drm/xocl/mgmtpf/mgmt-cw.c \
 driver/xclng/drm/xocl/mgmtpf/mgmt-utils.c \
 driver/xclng/drm/xocl/mgmtpf/mgmt-ioctl.c \
 driver/xclng/drm/xocl/mgmtpf/mgmt-sysfs.c \
 driver/xclng/drm/xocl/mgmtpf/mgmt-core.h \
 driver/xclng/drm/xocl/mgmtpf/10-xclmgmt.rules  \
 driver/xclng/drm/xocl/mgmtpf/Makefile \
 driver/xclng/drm/xocl/xocl_drv.h \
 driver/xclng/drm/xocl/xocl_subdev.c \
 driver/xclng/drm/xocl/xocl_ctx.c \
 driver/xclng/drm/xocl/xocl_thread.c \
 driver/xclng/drm/xocl/xocl_test.c \
 driver/xclng/drm/xocl/userpf/xdma.c \
 driver/xclng/drm/xocl/userpf/qdma.c \
 driver/xclng/drm/xocl/userpf/common.h \
 driver/xclng/drm/xocl/userpf/xocl_bo.c \
 driver/xclng/drm/xocl/userpf/xocl_bo.h \
 driver/xclng/drm/xocl/userpf/xocl_drm.c \
 driver/xclng/drm/xocl/userpf/xocl_drm.h \
 driver/xclng/drm/xocl/userpf/xocl_ioctl.c \
 driver/xclng/drm/xocl/userpf/xocl_sysfs.c \
 driver/xclng/drm/xocl/userpf/xocl_drv.c \
 driver/xclng/drm/xocl/userpf/10-xocl.rules  \
 driver/xclng/drm/xocl/userpf/Makefile \
 driver/xclng/drm/xocl/lib/libxdma.c \
 driver/xclng/drm/xocl/lib/libxdma.h \
 driver/xclng/drm/xocl/lib/cdev_sgdma.h \
 driver/xclng/drm/xocl/lib/libxdma_api.h \
 driver/xclng/drm/xocl/lib/Makefile.in \
 driver/xclng/drm/xocl/lib/libqdma/libqdma_export.h \
 driver/xclng/drm/xocl/lib/libqdma/libqdma_export.c \
 driver/xclng/drm/xocl/lib/libqdma/libqdma_config.h \
 driver/xclng/drm/xocl/lib/libqdma/libqdma_config.c \
 driver/xclng/drm/xocl/lib/libqdma/qdma_descq.c \
 driver/xclng/drm/xocl/lib/libqdma/qdma_descq.h \
 driver/xclng/drm/xocl/lib/libqdma/qdma_device.c \
 driver/xclng/drm/xocl/lib/libqdma/qdma_device.h \
 driver/xclng/drm/xocl/lib/libqdma/qdma_intr.c \
 driver/xclng/drm/xocl/lib/libqdma/qdma_intr.h \
 driver/xclng/drm/xocl/lib/libqdma/qdma_context.c \
 driver/xclng/drm/xocl/lib/libqdma/qdma_context.h \
 driver/xclng/drm/xocl/lib/libqdma/qdma_mbox.c \
 driver/xclng/drm/xocl/lib/libqdma/qdma_mbox.h \
 driver/xclng/drm/xocl/lib/libqdma/qdma_sriov.c \
 driver/xclng/drm/xocl/lib/libqdma/qdma_st_c2h.c \
 driver/xclng/drm/xocl/lib/libqdma/thread.c \
 driver/xclng/drm/xocl/lib/libqdma/thread.h \
 driver/xclng/drm/xocl/lib/libqdma/qdma_thread.c \
 driver/xclng/drm/xocl/lib/libqdma/qdma_thread.h \
 driver/xclng/drm/xocl/lib/libqdma/version.h \
 driver/xclng/drm/xocl/lib/libqdma/qdma_compat.h \
 driver/xclng/drm/xocl/lib/libqdma/xdev.h \
 driver/xclng/drm/xocl/lib/libqdma/xdev.c \
 driver/xclng/drm/xocl/lib/libqdma/qdma_regs.h \
 driver/xclng/drm/xocl/lib/libqdma/qdma_regs.c \
 driver/xclng/drm/xocl/subdev/feature_rom.c \
 driver/xclng/drm/xocl/subdev/mm_xdma.c \
 driver/xclng/drm/xocl/subdev/mm_qdma.c \
 driver/xclng/drm/xocl/subdev/mb_scheduler.c \
 driver/xclng/drm/xocl/subdev/xvc.c \
 driver/xclng/drm/xocl/subdev/sysmon.c \
 driver/xclng/drm/xocl/subdev/firewall.c \
 driver/xclng/drm/xocl/subdev/microblaze.c \
 driver/xclng/drm/xocl/subdev/xiic.c \
 driver/xclng/drm/xocl/subdev/mailbox.c \
 driver/xclng/drm/xocl/subdev/icap.c \
 driver/xclng/drm/xocl/subdev/str_qdma.c \
 driver/xclng/drm/.dir-locals.el \
 driver/xclng/include/xocl_ioctl.h  \
 driver/xclng/include/mgmt-reg.h  \
 driver/xclng/include/mgmt-ioctl.h  \
 driver/xclng/include/qdma_ioctl.h  \
 driver/xclng/include/devices.h  \
 driver/xclng/include/drm/drm.h  \
 driver/xclng/include/drm/drm_mode.h \
 driver/include/ert.h \
 driver/include/xclfeatures.h \
 driver/include/xclbin.h \
 driver/include/xclerr.h

XOCL_DRV_ARCHIVE_NAME := xocl.zip
ZIP_SRC := $(addprefix $(SDACCEL_RUNTIME_DIR)/, $(XOCL_KERNEL_DRV_SRC))
ZIP_TAR := $(RDI_BUILD_DIR)/$(SSNAME)/$(SSDIR)/$(XOCL_DRV_ARCHIVE_NAME)
$(ZIP_TAR) : $(ZIP_SRC) $(SSMKFILE) | $(dir $(ZIP_TAR)).rdi
	cd $(SDACCEL_RUNTIME_DIR); $(_ZIP) $@ $(XOCL_KERNEL_DRV_SRC)

zip: $(ZIP_TAR)

ifeq ($(RDI_PLATFORM),lnx64)

 XCLNG_SUBDIR := sw/driver/gem

 # release zipped sources to each driver release dir
 $(foreach dir,$(XCLNG_RELEASE_DIRS),\
  $(eval $(call XCLNG_release_data_template,$(dir)/$(XCLNG_SUBDIR),$(ZIP_TAR))))

 # release zipped sources to data/sdaccel/pcie/src
 $(foreach dir,data/sdaccel/pcie/src,\
  $(eval $(call XCLNG_release_data_template,$(dir),$(ZIP_TAR))))

 # release zipped sources to each driver internal release dir
 $(foreach dir,$(XCLNG_INTERNAL_RELEASE_DIRS),\
  $(eval $(call XCLNG_internal_release_data_template,$(dir)/$(XCLNG_SUBDIR),$(ZIP_TAR))))

 # release zipped sources to each aws driver internal release dir
 $(foreach dir,$(AWS_XCLNG_INTERNAL_RELEASE_DIRS),\
  $(eval $(call XCLNG_internal_release_data_template,$(dir)/$(XCLNG_SUBDIR),$(ZIP_TAR))))

endif

include $(RDI_MAKEROOT)/bottom.mk
