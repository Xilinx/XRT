include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 ppc64le arm64
include $(RDI_MAKEROOT)/toolset.mk

MYINCLUDES += \
 $(RDI_SRCROOT)/products/sdx/ocl/src/include/1_2 \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/user \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include 

ifeq ($(RDI_PLATFORM),arm64)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/arm.mk
 MYCFLAGS := $(ARM_CXXFLAGS)
endif #ifeq ($(RDI_PLATFORM),arm64)


ifeq ($(RDI_PLATFORM),lnx64)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/x86.mk
 MYCFLAGS := $(X86_CXXDEFINEFLAGS) -pthread
endif

# ppc64le
ifeq ($(RDI_PLATFORM),ppc64le)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/ppc.mk
 MYCFLAGS := $(PPC_CXXDEFINEFLAGS) -pthread
endif

# aarch64
ifeq ($(RDI_PLATFORM),aarch64)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/aarch.mk
 MYCFLAGS := $(AARCH_CXXDEFINEFLAGS) -pthread
endif

CPP_SUFFIX := .cpp
include $(RDI_MAKEROOT)/objs.mk

include $(RDI_MAKEROOT)/bottom.mk
