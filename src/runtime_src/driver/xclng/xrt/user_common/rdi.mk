include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 arm64 ppc64le
include $(RDI_MAKEROOT)/toolset.mk

MYINCLUDES := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src

MYCFLAGS := -Wall -fvisibility=hidden
MYCFLAGS += -DXCLHAL_MAJOR_VER=$(MAJOR_VER) -DXCLHAL_MINOR_VER=$(MINOR_VER)
MYCFLAGS += -DOSNAME="Linux" -DLINUX -DPOSIX

CPP_SUFFIX := .cpp
include $(RDI_MAKEROOT)/objs.mk

LIBRARY_NAME := xclng_xrt_common_static
include $(RDI_MAKEROOT)/stlib.mk

include $(RDI_MAKEROOT)/bottom.mk
