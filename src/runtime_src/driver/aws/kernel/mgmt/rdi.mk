include $(RDI_MAKEROOT)/top.mk
include $(RDI_MAKEROOT)/platform.mk

SDACCEL_RUNTIME_DIR := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src

AWSMGMT_KERNEL_DRV_SRC := \
 driver/aws/kernel/mgmt/mgmt-cw.h         \
 driver/include/xclbin.h                  \
 driver/include/xclerr.h                  \
 driver/xclng/include/mgmt-ioctl.h        \
 driver/aws/kernel/mgmt/mgmt-bit.h        \
 driver/aws/kernel/mgmt/Makefile          \
 driver/aws/kernel/mgmt/mgmt-bit.c        \
 driver/aws/kernel/mgmt/mgmt-core.h       \
 driver/aws/kernel/mgmt/mgmt-core.c       \
 driver/aws/kernel/mgmt/mgmt-cw.c         \
 driver/aws/kernel/mgmt/10-awsmgmt.rules  \
 driver/aws/kernel/mgmt/mgmt-thread.c	  \
 driver/aws/kernel/mgmt/mgmt-firewall.c   \
 driver/aws/kernel/mgmt/mgmt-sysfs.c

AWSMGMT_DRV_ARCHIVE_NAME := awsmgmt.zip
ZIP_SRC := $(addprefix $(SDACCEL_RUNTIME_DIR)/, $(AWSMGMT_KERNEL_DRV_SRC))
ZIP_TAR := $(RDI_BUILD_DIR)/$(SSNAME)/$(SSDIR)/$(AWSMGMT_DRV_ARCHIVE_NAME)
$(ZIP_TAR) : $(ZIP_SRC) $(SSMKFILE) | $(dir $(ZIP_TAR)).rdi
	cd $(SDACCEL_RUNTIME_DIR); $(_ZIP) $@ $(AWSMGMT_KERNEL_DRV_SRC)

# Release zip file specified directory as a data file
# $(1) : releasedir
# $(2) : file to release
define AWSMGMT_ZIP_release_data_template
 RELEASE_DATA_DIR := $(1)
 RELEASE_DATA_EXACT := $(2)
 RELEASE_TYPE := internal
 include $(RDI_MAKEROOT)/release.mk
endef

ifeq ($(RDI_PLATFORM),lnx64)

 AWSMGMT_ZIP_RELEASE_DIRS := \
  platforms/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_5_0/sw/driver \
  platforms/xilinx_aws-vu9p-f1_dynamic_5_0/sw/driver \
  platforms/xilinx_aws-vu9p-f1-04261818_dynamic_5_0/sw/driver \

 # release zipped sources to each driver release dir and data/sdaccel/pcie/src
 $(foreach dir,$(AWSMGMT_ZIP_RELEASE_DIRS) data/sdaccel/pcie/src,\
  $(eval $(call AWSMGMT_ZIP_release_data_template,$(dir),$(ZIP_TAR))))

endif

include $(RDI_MAKEROOT)/bottom.mk
