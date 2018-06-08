include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := arm64 aarch64

include $(RDI_MAKEROOT)/toolset.mk

MAJOR_VER := 1
MINOR_VER := 1
MYINCLUDES := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/zynq/user \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include \
 $(RDI_INCLUDE_DIR)/ocl/runtime_src/driver/common_em \
 $(RDI_SRCROOT)/products/sdx/ocl/src/emulation/common
 
MYCFLAGS := -Wall -fvisibility=hidden
MYCFLAGS += -DXCLHAL_MAJOR_VER=$(MAJOR_VER) -DXCLHAL_MINOR_VER=$(MINOR_VER)
MYCFLAGS += -DOSNAME="Linux" -DLINUX -DPOSIX -D__HWEM__

CPP_SUFFIX := .cpp
OBJS_PROPAGATE := no
include $(RDI_MAKEROOT)/objs.mk
include $(RDI_SRCROOT)/products/sdx/ocl/rdi/static_stdcpp.mk
STLIBRARIES := \
 products/sdx/rdi_pic_schema_static \
 products/sdx/sdx_lmx6.0_static 
LIBRARY_NAME := hw_em 
MYFINALLINK += -lrt
ifeq ($(RDI_PLATFORM),arm64)
include $(RDI_MAKEROOT)/shlib.mk
 RELEASE_LIBS_DIR := data/emulation/unified/hw_em/zynq/driver/
 RELEASE_LIBS := $(LIBRARY_NAME)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endif

ifeq ($(RDI_PLATFORM),aarch64)
include $(RDI_MAKEROOT)/shlib.mk
 RELEASE_LIBS_DIR := data/emulation/unified/hw_em/zynqu/driver/
 RELEASE_LIBS := $(LIBRARY_NAME)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endif


#Temporary workaround for enabling Hw_emulation flow and should be removed once real driver is available
LIBRARY_NAME := cpu_em 
ifeq ($(RDI_PLATFORM),arm64)
include $(RDI_MAKEROOT)/shlib.mk
 RELEASE_LIBS_DIR := data/emulation/unified/cpu_em/zynq/driver/
 RELEASE_LIBS := $(LIBRARY_NAME)
 RELEASE_TYPE := customer nostrip
 include $(RDI_MAKEROOT)/release.mk
endif

include $(RDI_MAKEROOT)/bottom.mk
