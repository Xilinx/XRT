include $(RDI_MAKEROOT)/top.mk
SUBDIR_EXCLUDE := zynq
include $(RDI_MAKEROOT)/subdir.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 arm64 ppc64le
include $(RDI_MAKEROOT)/toolset.mk

MYINCLUDES := \
 $(RDI_SRCROOT)/products/sdx/ocl/src/include/1_2 \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/ \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/user \
 $(RDI_SRCROOT)/products/hls/src/hwsyn/pass/opencl/include

# arm
ifeq ($(RDI_PLATFORM),arm64)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/arm.mk
 MYCFLAGS += $(ARM_CXXFLAGS) -pthread
endif # ifeq ($(RDI_PLATFORM),arm64)

# x86
ifeq ($(RDI_PLATFORM),lnx64)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/x86.mk
 MYCFLAGS += $(X86_CXXDEFINEFLAGS) -pthread
 ifdef ENABLE_PMD
   MYCFLAGS += -DPMD_OCL
 endif
endif

# ppc64le
ifeq ($(RDI_PLATFORM),ppc64le)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/ppc.mk
 MYCFLAGS += $(PPC_CXXDEFINEFLAGS) -pthread
endif

# aarch64
ifeq ($(RDI_PLATFORM),aarch64)
 include $(RDI_SRCROOT)/products/sdx/ocl/rdi/aarch.mk
 MYCFLAGS += $(AARCH_CXXDEFINEFLAGS) -pthread
endif

CPP_SUFFIX := .cpp
include $(RDI_MAKEROOT)/objs.mk

# once only
ifndef $(SSID)/rdi.mk
 $(SSID)/rdi.mk := defined
 RELEASE_DATA_DIR := common/technology/autopilot/opencl
 RELEASE_DATA_EXACT := $(RDI_SRCROOT)/$(SSNAME)/sdx/ocl/src/runtime_src/impl/cpu_pipes.h
 RELEASE_TYPE := customer
 include $(RDI_MAKEROOT)/release.mk
endif

include $(RDI_MAKEROOT)/bottom.mk
