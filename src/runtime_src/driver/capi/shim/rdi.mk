include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := ppc64le
include $(RDI_MAKEROOT)/toolset.mk

ifeq ($(RDI_PLATFORM),ppc64le)

 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/ppc.mk
 MYINCLUDES := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include
 MAJOR_VER := 1
 MINOR_VER := 0
 MYCFLAGS := -Wall -fvisibility=hidden -DXCLHAL_MAJOR_VER=$(MAJOR_VER) -DXCLHAL_MINOR_VER=$(MINOR_VER)
 MYCFLAGS += -DOSNAME="Linux" -DLINUX -DPOSIX
 MYCFLAGS += $(PPC_CXXFLAGS)
 CPP_SUFFIX := .cpp
 include $(RDI_MAKEROOT)/objs.mk

 LIBRARY_NAME := capidrv
 SHLIBRARIES := \
  products/sdx/cxl
 MYFINALLINK += -lrt
 include $(RDI_MAKEROOT)/shlib.mk

# RELEASE_LIBS_DIR := data/sdaccel/pcie/ppc64le
# RELEASE_LIBS := $(LIBRARY_NAME)
# RELEASE_TYPE := customer nostrip
# include $(RDI_MAKEROOT)/release.mk

endif # ppc64le

include $(RDI_MAKEROOT)/bottom.mk
