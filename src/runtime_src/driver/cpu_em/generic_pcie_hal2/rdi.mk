include $(RDI_MAKEROOT)/top.mk
include $(RDI_MAKEROOT)/subdir.mk
TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64
include $(RDI_MAKEROOT)/toolset.mk

MAJOR_VER := 1
MINOR_VER := 0
MYCFLAGS := -Wall -fvisibility=hidden -DXCLHAL_MAJOR_VER=$(MAJOR_VER) -DXCLHAL_MINOR_VER=$(MINOR_VER) -DUSE_HAL2 
MYCFLAGS += -DOSNAME="Linux" -DLINUX -DPOSIX
MYINCLUDES := \
  $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/cpu_em/generic_pcie_hal2 \
  $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include \
  $(RDI_SRCROOT)/products/hls/src/hwsyn/pass/opencl/include
OBJS_PROPAGATE := no
include $(RDI_MAKEROOT)/objs.mk

include $(RDI_SRCROOT)/products/sdx/ocl/rdi/static_stdcpp.mk
MYFINALLINK := $(STATIC_STDCPP) -ldl -lpthread -lcrypt
MYFINALLINK += -Wl,-Bsymbolic
STLIBRARIES := \
 products/sdx/rdi_pic_schema_static \
 products/sdx/rdi_common_em_static \
 products/sdx/sdx_lmx6.0_static \
 ext/protobuf_static

LIBRARY_NAME := cpu_em
include $(RDI_MAKEROOT)/shlib.mk

ifeq ($(RDI_PLATFORM),lnx64)
 RELEASE_LIBS_DIR := data/emulation/unified/cpu_em/generic_pcie/driver
 RELEASE_LIBS := $(LIBRARY_NAME)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endif

ifeq ($(RDI_PLATFORM),aarch64)
 RELEASE_LIBS_DIR := data/emulation/unified/cpu_em/zynqu/driver/
 RELEASE_LIBS := $(LIBRARY_NAME)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endif

include $(RDI_MAKEROOT)/bottom.mk
