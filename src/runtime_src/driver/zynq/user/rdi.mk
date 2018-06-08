include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := arm64 aarch64

include $(RDI_MAKEROOT)/toolset.mk

MAJOR_VER := 1
MINOR_VER := 1

MYINCLUDES := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include
MYCFLAGS := -O2 -Wall -fvisibility=hidden
MYCFLAGS += -DXCLHAL_MAJOR_VER=$(MAJOR_VER) -DXCLHAL_MINOR_VER=$(MINOR_VER)
MYCFLAGS += -DOSNAME="Linux" -DLINUX -DPOSIX

CPP_SUFFIX := .cpp
include $(RDI_MAKEROOT)/objs.mk

LIBRARY_NAME := xclzynqdrv_static
include $(RDI_MAKEROOT)/stlib.mk

LIBRARY_NAME := xclzynqdrv
MYFINALLINK += -lrt
include $(RDI_MAKEROOT)/shlib.mk

ifeq ($(RDI_PLATFORM),lnx64)
 ZYNQ_HAL_DRV_SRC := \
  driver/include/xclhal2.h \
  driver/include/xclerr.h \
  driver/zynq/user/shim.cpp \
  driver/zynq/user/shim.h \
  driver/zynq/user/NOTICE \
  driver/zynq/user/README \
  driver/zynq/include/drm.h \
  driver/zynq/include/drm_mode.h \
  driver/zynq/include/zynq_ioctl.h \
  driver/zynq/user/Makefile

 ZYNQ_ZIP_SRC := $(addprefix $(SDACCEL_RUNTIME_DIR)/,$(ZYNQ_HAL_DRV_SRC))
 ZYNQ_ZIP_TAR := $(RDI_BUILD_DIR)/$(SSNAME)/$(SSDIR)/zynqhal.zip

 $(ZYNQ_ZIP_TAR) : $(ZYNQ_ZIP_SRC) | $(dir $(ZYNQ_ZIP_TAR)).rdi
	cd $(SDACCEL_RUNTIME_DIR); $(_ZIP) $@ $(ZYNQ_HAL_DRV_SRC)

 RELEASE_DATA_TARGET := release
 RELEASE_DATA_DIR := data/sdaccel/zynq/src
 RELEASE_TYPE := customer
 RELEASE_DATA_EXACT := $(ZYNQ_ZIP_TAR)
 include $(RDI_MAKEROOT)/release.mk

endif

ifeq ($(RDI_PLATFORM),arm64)
 RELEASE_LIBS_DIR := data/sdaccel/zynq/arm
 RELEASE_LIBS := $(LIBRARY_NAME)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endif

ifeq ($(RDI_PLATFORM),aarch64)
 RELEASE_LIBS_DIR := data/sdaccel/zynq/aarch64
 RELEASE_LIBS := $(LIBRARY_NAME)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endif

include $(RDI_MAKEROOT)/bottom.mk
