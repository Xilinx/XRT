include $(RDI_MAKEROOT)/top.mk

ifeq ($(RDI_PLATFORM),lnx64)
#TOOLSET_HOST := lnx64
#include $(RDI_MAKEROOT)/toolset.mk

MYCFLAGS += -Wall -fvisibility=hidden
MYCFLAGS += -DINTERNAL_TESTING

SDACCEL_RUNTIME_DIR := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src
MYINCLUDES += $(SDACCEL_RUNTIME_DIR) $(SDACCEL_RUNTIME_DIR)/driver/xclng/xrt/user_aws/
CPP_SUFFIX := .cpp
OBJS_PROPAGATE := no

include $(RDI_MAKEROOT)/objs.mk

EXEC_NAME := awssak_gem
EXEC_OBJS := main
STLIBRARIES := products/sdx/awsbmdrv_static products/sdx/xclng_xrt_common_static
MYEXECLFLAGS := -static-libstdc++
MYFINALLINK := -lrt
include $(RDI_MAKEROOT)/exec.mk

RELEASE_EXECS := $(EXEC_NAME)
RELEASE_EXECS_DIR := runtime/bin
ifneq ($(RDI_PLATFORM),lnx64)
 RELEASE_EXECS_DIR := $(RELEASE_EXECS_DIR)/$(RDI_PLATFORM)
endif
RELEASE_TYPE := internal
include $(RDI_MAKEROOT)/release.mk

endif


EXEC_RELEASE_NAME := xbsak
EXEC_FULL_NAME := $(RDI_BUILD_DIR)/products/sdx/$(EXEC_NAME)

# Release executable to specified directory
# $(1) : releasedir
# $(2) : file to release
define AWSSAK_release_exe_template
 RELEASE_EXECS_DIR := $(1)
 RELEASE_EXECS_RENAME_EXACT_FROM := $(2)
 RELEASE_EXECS_RENAME_TO := $(EXEC_RELEASE_NAME)
 RELEASE_TYPE := internal nostrip
 include $(RDI_MAKEROOT)/release.mk
endef

ifeq ($(RDI_PLATFORM),lnx64)
 XCLNG_BIN_RELEASE_DIRS := \
  platforms/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_5_0/sw/bin/gem \
  platforms/xilinx_aws-vu9p-f1_dynamic_5_0/sw/bin/gem \
  platforms/xilinx_aws-vu9p-f1-04261818_dynamic_5_0/sw/bin/gem \

 # release xbsak_ng platform/sw/bin dir
 $(foreach dir,$(XCLNG_BIN_RELEASE_DIRS),\
  $(eval $(call AWSSAK_release_exe_template,$(dir),$(EXEC_FULL_NAME))))
endif


include $(RDI_MAKEROOT)/bottom.mk
