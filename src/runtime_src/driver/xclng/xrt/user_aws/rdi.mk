include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := ppc64le
include $(RDI_MAKEROOT)/toolset.mk

MAJOR_VER := 2
MINOR_VER := 1

SDACCEL_RUNTIME_DIR := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src

MYINCLUDES := $(SDACCEL_RUNTIME_DIR)
MYINCLUDES += $(SDACCEL_RUNTIME_DIR)/driver/xclng/xrt/user_common
MYCFLAGS := -Wall -fvisibility=hidden
MYCFLAGS += -DXCLHAL_MAJOR_VER=$(MAJOR_VER) -DXCLHAL_MINOR_VER=$(MINOR_VER)
MYCFLAGS += -DOSNAME="Linux" -DLINUX -DPOSIX
MYCFLAGS += -DINTERNAL_TESTING
MYCFLAGS += -I$(SDACCEL_RUNTIME_DIR)/driver/xclng/include -I$(SDACCEL_RUNTIME_DIR)/driver/include
MYCFLAGS += -I$(SDACCEL_RUNTIME_DIR)/driver/aws/kernel/include -I$(SDACCEL_RUNTIME_DIR)/driver/xclng/include/drm

STLIBRARIES := products/sdx/xclng_xrt_common_static

CPP_SUFFIX := .cpp
include $(RDI_MAKEROOT)/objs.mk

LIBRARY_NAME := awsbmdrv_static
include $(RDI_MAKEROOT)/stlib.mk

include $(RDI_SRCROOT)/products/sdx/ocl/rdi/static_stdcpp.mk
MYFINALLINK := $(STATIC_STDCPP) -lrt

LIBRARY_NAME := awsbmgemdrv
include $(RDI_MAKEROOT)/shlib.mk

AWSXCL_HAL_DRV_SRC := \
 driver/include/xclhal2.h \
 driver/include/xclerr.h \
 driver/include/xcl_app_debug.h \
 driver/include/xcl_axi_checker_codes.h \
 driver/include/xclperf.h \
 driver/include/xclbin.h \
 driver/xclng/include/mgmt-ioctl.h \
 driver/xclng/include/xocl_ioctl.h \
 driver/xclng/include/drm/drm.h \
 driver/xclng/include/drm/drm_mode.h \
 driver/xclng/xrt/user_aws/Makefile \
 driver/xclng/xrt/user_aws/awssak.cpp \
 driver/xclng/xrt/user_aws/awssak.h \
 driver/xclng/xrt/user_aws/awssak_debug.cpp \
 driver/xclng/xrt/user_aws/debug.cpp \
 driver/xclng/xrt/user_aws/perf.cpp \
 driver/xclng/xrt/user_aws/scan.cpp \
 driver/xclng/xrt/user_aws/scan.h \
 driver/xclng/xrt/user_aws/shim.cpp \
 driver/xclng/xrt/user_aws/shim.h \
 driver/xclng/xrt/user_aws/xclbin.cpp \
 driver/xclng/tools/awssak/Makefile \
 driver/xclng/tools/awssak/main.cpp \
 driver/xclng/xrt/user_common/dd.cpp \
 driver/xclng/xrt/user_common/dd.h \
 driver/xclng/xrt/user_common/dmatest.h \
 driver/xclng/xrt/user_common/memaccess.h \
 driver/xclng/xrt/user_common/perfmon_parameters.h \
 driver/xclng/xrt/user_common/utils.cpp \
 driver/xclng/xrt/user_common/utils.h 


AWSXCL_ZIP_SRC := $(addprefix $(SDACCEL_RUNTIME_DIR)/,$(AWSXCL_HAL_DRV_SRC))
AWSXCL_ZIP_TAR := $(RDI_BUILD_DIR)/$(SSNAME)/$(SSDIR)/awsbmgemhal.zip

$(AWSXCL_ZIP_TAR) : $(AWSXCL_ZIP_SRC) $(SSMKFILE) | $(dir $(AWSXCL_ZIP_TAR)).rdi
	cd $(SDACCEL_RUNTIME_DIR); $(_ZIP) $@ $(AWSXCL_HAL_DRV_SRC)

# Release library to specified directory
# $(1) : releasedir
# $(2) : file to release
define AWSXCL_release_libs_template
 RELEASE_LIBS_DIR := $(1)
 RELEASE_LIBS := $(2)
 RELEASE_TYPE := internal nostrip
 include $(RDI_MAKEROOT)/release.mk
endef

# Release zip file specified directory as a data file
# $(1) : releasedir
# $(2) : file to release
define AWSXCL_release_data_template
 RELEASE_DATA_DIR := $(1)
 RELEASE_DATA_EXACT := $(2)
 RELEASE_TYPE := internal
 include $(RDI_MAKEROOT)/release.mk
endef

ifeq ($(RDI_PLATFORM),lnx64)
 AWSXCL_DRIVER_RELEASE_DIRS := \
  platforms/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_5_0/sw/driver/gem \
  platforms/xilinx_aws-vu9p-f1_dynamic_5_0/sw/driver/gem \
  platforms/xilinx_aws-vu9p-f1-04261818_dynamic_5_0/sw/driver/gem \

 # release driver to each driver release dir and data/sdaccel/pcie/x86_64
 $(foreach dir,$(AWSXCL_DRIVER_RELEASE_DIRS) data/sdaccel/pcie/x86_64,\
  $(eval $(call AWSXCL_release_libs_template,$(dir),$(LIBRARY_NAME))))

 # release zipped sources to each driver release dir and data/sdaccel/pcie/src
 $(foreach dir,$(AWSXCL_DRIVER_RELEASE_DIRS) data/sdaccel/pcie/src,\
  $(eval $(call AWSXCL_release_data_template,$(dir),$(AWSXCL_ZIP_TAR))))
endif

include $(RDI_MAKEROOT)/bottom.mk
