include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 arm64 ppc64le
include $(RDI_MAKEROOT)/toolset.mk

# The python gdb extensions are implemented in appdebug.py 
# and this need to put in scripts/ in release area.  
# Data files should be released only on one platform.
ifeq ($(RDI_PLATFORM),lnx64)
 RELEASE_DATA_FILES := appdebug.py appdebugint.py
 RELEASE_DATA_DIR := scripts
 RELEASE_TYPE := customer
 include $(RDI_MAKEROOT)/release.mk
endif

MYINCLUDES := \
 $(RDI_SRCROOT)/products/sdx/ocl/src/include/1_2 \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/user \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include

MYCFLAGS := -DCL_USE_DEPRECATED_OPENCL_1_1_APIS -DCL_USE_DEPRECATED_OPENCL_1_0_APIS

# arm
ifeq ($(RDI_PLATFORM),arm64)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/arm.mk
 MYCFLAGS += $(ARM_CXXFLAGS) -pthread
endif # ifeq ($(RDI_PLATFORM),arm64)

# ppc
ifeq ($(RDI_PLATFORM),ppc64le)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/ppc.mk
 MYCFLAGS += $(PPC_CXXFLAGS) -pthread
endif # ifeq ($(RDI_PLATFORM),ppc64le)

# ppc
ifeq ($(RDI_PLATFORM),aarch64)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/aarch.mk
 MYCFLAGS += $(AARCH_CXXFLAGS) -pthread
endif # ifeq ($(RDI_PLATFORM),ppc64le)

# x86
ifeq ($(RDI_PLATFORM),lnx64)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/x86.mk
 MYCFLAGS += $(X86_CXXDEFINEFLAGS) -pthread
 ifdef ENABLE_PMD
   MYCFLAGS += -DPMD_OCL
 endif
endif

#App Debug module requires more visibility, so force -g flag
MYCFLAGS += -g
OBJS_PROPAGATE := yes

CPP_SUFFIX := .cpp
include $(RDI_MAKEROOT)/objs.mk

include $(RDI_MAKEROOT)/bottom.mk
