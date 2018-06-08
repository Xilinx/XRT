include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := ppc64le aarch64
include $(RDI_MAKEROOT)/toolset.mk

MAJOR_VER := 2
MINOR_VER := 0

SDACCEL_RUNTIME_DIR := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src
include $(SDACCEL_RUNTIME_DIR)/driver/xclng/xclng_release.mk

MYINCLUDES := $(SDACCEL_RUNTIME_DIR) $(SDACCEL_RUNTIME_DIR)/driver/include $(SDACCEL_RUNTIME_DIR)/driver/xclng/xrt/user_common
MYCFLAGS := -Wall -fvisibility=hidden
MYCFLAGS += -DXCLHAL_MAJOR_VER=$(MAJOR_VER) -DXCLHAL_MINOR_VER=$(MINOR_VER)
MYCFLAGS += -DOSNAME="Linux" -DLINUX -DPOSIX

STLIBRARIES := products/sdx/xclng_xrt_common_static

CPP_SUFFIX := .cpp
include $(RDI_MAKEROOT)/objs.mk

LIBRARY_NAME := xclgemdrv_static
include $(RDI_MAKEROOT)/stlib.mk

include $(RDI_SRCROOT)/products/sdx/ocl/rdi/static_stdcpp.mk
MYFINALLINK := $(STATIC_STDCPP) -lrt -lpthread

LIBRARY_NAME := xclgemdrv
include $(RDI_MAKEROOT)/shlib.mk

XCLGEM_HAL_DRV_SRC := \
 driver/include/xclhal2.h \
 driver/include/xclerr.h \
 driver/include/xcl_app_debug.h \
 driver/include/xcl_axi_checker_codes.h \
 driver/include/xclperf.h \
 driver/include/xclbin.h \
 driver/xclng/xrt/user_gem/Makefile \
 driver/xclng/xrt/user_gem/perf.cpp \
 driver/xclng/xrt/user_gem/debug.cpp \
 driver/xclng/xrt/user_gem/shim.cpp \
 driver/xclng/xrt/user_gem/scan.cpp \
 driver/xclng/xrt/user_gem/hwmon.cpp \
 driver/xclng/xrt/user_gem/xbsak_debug.cpp \
 driver/xclng/xrt/user_gem/xbsak.cpp \
 driver/xclng/xrt/user_gem/shim.h \
 driver/xclng/xrt/user_gem/scan.h \
 driver/xclng/xrt/user_gem/hwmon.h \
 driver/xclng/xrt/user_gem/xbsak.h \
 driver/xclng/xrt/user_gem/NOTICE \
 driver/xclng/xrt/user_gem/LICENSE \
 driver/xclng/include/mgmt-ioctl.h \
 driver/xclng/include/xocl_ioctl.h \
 driver/xclng/include/mgmt-reg.h \
 driver/xclng/include/drm/drm.h \
 driver/xclng/include/drm/drm_mode.h \
 driver/xclng/tools/xbsak_gem/main.cpp \
 driver/xclng/tools/xbsak_gem/Makefile \
 driver/xclng/tools/xbsak_gem/LICENSE \
 driver/xclng/xrt/user_common/dd.cpp \
 driver/xclng/xrt/user_common/dd.h \
 driver/xclng/xrt/user_common/dmatest.h \
 driver/xclng/xrt/user_common/memaccess.h \
 driver/xclng/xrt/user_common/perfmon_parameters.h \
 driver/xclng/xrt/user_common/utils.cpp \
 driver/xclng/xrt/user_common/utils.h 

XCLGEM_ZIP_SRC := $(addprefix $(SDACCEL_RUNTIME_DIR)/,$(XCLGEM_HAL_DRV_SRC))
XCLGEM_ZIP_TAR := $(RDI_BUILD_DIR)/$(SSNAME)/$(SSDIR)/xclgemhal.zip

$(XCLGEM_ZIP_TAR) : $(XCLGEM_ZIP_SRC) $(SSMKFILE) | $(dir $(XCLGEM_ZIP_TAR)).rdi
	cd $(SDACCEL_RUNTIME_DIR); $(_ZIP) $@ $(XCLGEM_HAL_DRV_SRC)

ifeq ($(RDI_PLATFORM),lnx64)

 XCLNG_SUBDIR := sw/driver/gem

 # release driver to each driver release dir
 $(foreach dir,$(XCLNG_RELEASE_DIRS),\
  $(eval $(call XCLNG_release_libs_template,$(dir)/$(XCLNG_SUBDIR),$(LIBRARY_NAME))))

 # release driver to data/sdaccel/pcie/x86_64
 $(foreach dir,data/sdaccel/pcie/x86_64,\
  $(eval $(call XCLNG_release_libs_template,$(dir),$(LIBRARY_NAME))))

 # release zipped sources to each driver release dir
 $(foreach dir,$(XCLNG_RELEASE_DIRS),\
  $(eval $(call XCLNG_release_data_template,$(dir)/$(XCLNG_SUBDIR),$(XCLGEM_ZIP_TAR))))

 # release zipped sources to data/sdaccel/pcie/src
 $(foreach dir,data/sdaccel/pcie/src,\
  $(eval $(call XCLNG_release_data_template,$(dir),$(XCLGEM_ZIP_TAR))))

 # release driver to each driver internal release dir
 $(foreach dir,$(XCLNG_INTERNAL_RELEASE_DIRS),\
  $(eval $(call XCLNG_internal_release_libs_template,$(dir)/$(XCLNG_SUBDIR),$(LIBRARY_NAME))))

 # release zipped sources to each driver internal release dir
 $(foreach dir,$(XCLNG_INTERNAL_RELEASE_DIRS),\
  $(eval $(call XCLNG_internal_release_data_template,$(dir)/$(XCLNG_SUBDIR),$(XCLGEM_ZIP_TAR))))

endif

include $(RDI_MAKEROOT)/bottom.mk
