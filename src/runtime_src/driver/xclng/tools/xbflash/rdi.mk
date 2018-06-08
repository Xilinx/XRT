include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := ppc64le aarch64
include $(RDI_MAKEROOT)/toolset.mk

SDACCEL_RUNTIME_DIR := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src
include $(SDACCEL_RUNTIME_DIR)/driver/xclng/xclng_release.mk

MYCFLAGS := -Wall -fvisibility=hidden
MYINCLUDES := \
	$(SDACCEL_RUNTIME_DIR) \
	$(SDACCEL_RUNTIME_DIR)/driver/xclng/include/ \
	$(SDACCEL_RUNTIME_DIR)/driver/include/ \
	$(SDACCEL_RUNTIME_DIR)/driver/xclng/xrt/user_gem/ 
CPP_SUFFIX := .cpp
OBJS_PROPAGATE := no
include $(RDI_MAKEROOT)/objs.mk

EXEC_NAME := xbflash
EXEC_OBJS := xbflash \
             flasher \
			 xspi \
			 prom
STLIBRARIES := products/sdx/xclgemdrv_static products/sdx/xclng_xrt_common_static
MYEXECLFLAGS := -static-libstdc++
MYFINALLINK := -lrt -lpthread -fpic
include $(RDI_MAKEROOT)/exec.mk

ifeq ($(RDI_PLATFORM),lnx64)
  RELEASE_EXECS := $(EXEC_NAME)
  RELEASE_EXECS_DIR := runtime/bin
  RELEASE_TYPE := customer
  include $(RDI_MAKEROOT)/release.mk
endif

ifeq ($(RDI_PLATFORM),aarch64)
  RELEASE_EXECS := $(EXEC_NAME)
  RELEASE_EXECS_DIR := runtime/bin/$(RDI_PLATFORM)
  RELEASE_TYPE := internal
  include $(RDI_MAKEROOT)/release.mk
endif

ifeq ($(RDI_PLATFORM),ppc64le)
  RELEASE_EXECS := $(EXEC_NAME)
  RELEASE_EXECS_DIR := runtime/bin/$(RDI_PLATFORM)
  RELEASE_TYPE := internal
  include $(RDI_MAKEROOT)/release.mk
endif

ifeq ($(RDI_PLATFORM),lnx64)

 XCLNG_SUBDIR := sw/bin/gem

 # release xbflash platform/sw/bin dir
 $(foreach dir,$(XCLNG_RELEASE_DIRS),\
  $(eval $(call XCLNG_release_execs_rename_template,$(dir)/$(XCLNG_SUBDIR),$(EXEC),xbflash)))

 # same as above for internal
 # release xbflash platform/sw/bin dir
 $(foreach dir,$(XCLNG_INTERNAL_RELEASE_DIRS),\
  $(eval $(call XCLNG_internal_release_execs_rename_template,$(dir)/$(XCLNG_SUBDIR),$(EXEC),xbflash)))

endif

include $(RDI_MAKEROOT)/bottom.mk
