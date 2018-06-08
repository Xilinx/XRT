include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 arm64 ppc64le
include $(RDI_MAKEROOT)/toolset.mk

ifeq ($(RDI_OS),gnu/linux)
 MYCFLAGS := -Wall -Werror
endif

MYINCLUDES := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src
CPP_SUFFIX := .cpp
include $(RDI_MAKEROOT)/objs.mk

ifeq ($(RDI_PLATFORM),lnx64)
 RELEASE_DATA_DIR := runtime/driver/include
 RELEASE_DATA_FILES := task.h
 RELEASE_TYPE := internal
 include $(RDI_MAKEROOT)/release.mk
endif

include $(RDI_MAKEROOT)/bottom.mk
