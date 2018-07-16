include $(RDI_MAKEROOT)/top.mk
include $(RDI_MAKEROOT)/subdir.mk
LIB_NAME := cpu_em
TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64
include $(RDI_MAKEROOT)/toolset.mk

MAJOR_VER := 1
MINOR_VER := 0
MYCFLAGS := -Wall -fvisibility=hidden -DXCLHAL_MAJOR_VER=$(MAJOR_VER) -DXCLHAL_MINOR_VER=$(MINOR_VER) -DUSE_HAL2
MYCFLAGS += -DOSNAME="Linux" -DLINUX -DPOSIX

MYINCLUDES := \
  $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/$(LIB_NAME)/generic_pcie_hal2 \
  $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include \
  $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/common_em \
  $(RDI_SRCROOT)/products/hls/src/hwsyn/pass/opencl/include
OBJS_PROPAGATE := no
include $(RDI_MAKEROOT)/objs.mk

include $(RDI_SRCROOT)/products/sdx/ocl/rdi/static_stdcpp.mk
MYFINALLINK := $(STATIC_STDCPP)
MYFINALLINK += -Wl,-Bsymbolic
STLIBRARIES := \
 products/sdx/rdi_common_em_static \
 ext/protobuf_static

LIBRARY_NAME := $(LIB_NAME)
include $(RDI_MAKEROOT)/shlib.mk

ifeq ($(RDI_PLATFORM),lnx64)
 RELEASE_LIBS_DIR := data/emulation/unified/$(LIB_NAME)/generic_pcie/driver
 RELEASE_LIBS := $(LIBRARY_NAME)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endif

ifeq ($(RDI_PLATFORM),aarch64)
 RELEASE_LIBS_DIR := data/emulation/unified/$(LIB_NAME)/zynqu/driver/
 RELEASE_LIBS := $(LIBRARY_NAME)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endif

include $(RDI_MAKEROOT)/bottom.mk
