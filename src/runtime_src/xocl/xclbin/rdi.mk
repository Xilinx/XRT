include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 ppc64le arm64
include $(RDI_MAKEROOT)/toolset.mk

include $(RDI_MAKEROOT)/platform.mk

MYINCLUDES := \
 $(RDI_SRCROOT)/products/sdx/ocl/src/include/1_2 \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include

ifeq ($(RDI_OS),gnu/linux)
 MYCFLAGS := -Wall #-Werror
endif

ifeq ($(RDI_PLATFORM),lnx64)
 ifdef ENABLE_PMD
   MYCFLAGS += -DPMD_OCL
 endif
endif

CPP_SUFFIX := .cpp
include $(RDI_MAKEROOT)/objs.mk

include $(RDI_MAKEROOT)/bottom.mk
