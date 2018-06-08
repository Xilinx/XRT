include $(RDI_MAKEROOT)/top.mk
include $(RDI_MAKEROOT)/subdir.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 ppc64le
include $(RDI_MAKEROOT)/toolset.mk

MYINCLUDES := \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include

OBJS_PROPAGATE := no
CPP_SUFFIX := .cpp

ifeq ($(RDI_PLATFORM),lnx64)
 ifdef ENABLE_PMD
   MYCFLAGS += -DPMD_OCL
 endif
endif

include $(RDI_MAKEROOT)/objs.mk

EXEC_NAME := truntime
EXEC_OBJS := main
EXEC_EXTRA_OBJS := $(filter-out main, $(basename $(OBJS_$(SSDIR)_$(RDI_PLATFORM))))
SHLIBRARIES := \
 products/sdx/rdi_sdaccel_xrt
STLIBRARIES := \
 ext/boost_unit_test_framework_static
MYFINALLINK := -lpthread -ldl
include $(RDI_MAKEROOT)/exec.mk

RELEASE_EXECS := truntime
RELEASE_TYPE := internal
include $(RDI_MAKEROOT)/release.mk


include $(RDI_MAKEROOT)/bottom.mk
