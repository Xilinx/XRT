include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 arm64 ppc64le
include $(RDI_MAKEROOT)/toolset.mk

include $(RDI_MAKEROOT)/platform.mk

MYINCLUDES := \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src

ifeq ($(RDI_OS),gnu/linux)
 MYCFLAGS := -Wall -Werror
endif

CPP_SUFFIX := .cpp

include $(RDI_MAKEROOT)/objs.mk

include $(RDI_MAKEROOT)/bottom.mk
