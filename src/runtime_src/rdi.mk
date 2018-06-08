include $(RDI_MAKEROOT)/top.mk
include $(RDI_MAKEROOT)/subdir.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 arm64 ppc64le
include $(RDI_MAKEROOT)/toolset.mk

SDACCEL_RUNTIME_DIR := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src
include $(SDACCEL_RUNTIME_DIR)/driver/xclng/xclng_release.mk
RUNTIME_XCLNG_RELEASE_DIRS := \
 $(XCLNG_RELEASE_DIRS)
 
RUNTIME_XCLNG_INTERNAL_RELEASE_DIRS := \
 $(XCLNG_INTERNAL_RELEASE_DIRS) \
 $(AWS_XCLNG_INTERNAL_RELEASE_DIRS)

LIBRARY_NAME := xilinxopencl
STLIBRARIES := \
 products/sdx/rdi_sdaccel_xocl_static \
 products/sdx/rdi_sdaccel_xrt_static \
 products/sdx/rdi_pic_schema_static \
 products/sdx/sdx_lmx6.0_static \
 ext/boost_filesystem_static \
 ext/boost_system_static

include $(RDI_SRCROOT)/products/sdx/ocl/rdi/static_stdcpp.mk
MYFINALLINK := $(STATIC_STDCPP)
# Note "-Bsymbolic" is required so that the linker resolves all the OpenCL API calls
# in the runtime to implementations within the runtime. Otherwise a call to say clCreateKernel
# inside the runtime (api/clCreateKernelsInProgram.cpp:136) may resolve to an implementation
# in another runtime linked in by the host application via ICD.
#MYFINALLINK += -Wl,--version-script=$(SSCURDIR)/libxilinxopencl.map
MYFINALLINK += -Wl,-Bsymbolic -ldl -lpthread -lcrypt -lrt
MYLFLAGS := \
 -u xclGetMemObjDeviceAddress \
 -u xclGetMemObjectFromFd \
 -u xclGetMemObjectFd \
 -u xclEnqueuePeerToPeerCopyBuffer
include $(RDI_MAKEROOT)/shlib.mk

# arm
ifeq ($(RDI_PLATFORM),arm64)
 # Copy the ARM library to a revision version to run on Zynq
 ZYNQOPENCL_TAR := $(RDI_LINK_DIR)/products/sdx/libxilinxopencl$(SHLIBEXTN).1.0
 ZYNQOPENCL_SRC := $(RDI_LINK_DIR)/products/sdx/libxilinxopencl$(SHLIBEXTN)
 libs : $(ZYNQOPENCL_TAR)
 $(ZYNQOPENCL_TAR) : $(ZYNQOPENCL_SRC)
	$(CP) $< $@

 ifneq ($(debug),yes)
  RELEASE_LIBS_DIR := runtime/lib/zynq
  RELEASE_LIBS_EXACT := $(ZYNQOPENCL_TAR) $(ZYNQOPENCL_SRC)
  RELEASE_TYPE := customer nostrip
  include $(RDI_MAKEROOT)/release.mk
 endif
endif # ifeq ($(RDI_PLATFORM),arm64)

# x86
ifeq ($(RDI_PLATFORM),lnx64)
 ifneq ($(debug),yes)
  RELEASE_LIBS_DIR := runtime/lib/x86_64
  RELEASE_LIBS := $(LIBRARY_NAME)
  RELEASE_TYPE := customer nostrip
  include $(RDI_MAKEROOT)/release.mk

  # Release to platforms sw/lib dir
  $(foreach dir,$(RUNTIME_XCLNG_RELEASE_DIRS),\
   $(eval $(call XCLNG_release_libs_template,$(dir)/sw/lib/x86_64,$(LIBRARY_NAME))))

  $(foreach dir,$(RUNTIME_XCLNG_INTERNAL_RELEASE_DIRS),\
   $(eval $(call XCLNG_internal_release_libs_template,$(dir)/sw/lib/x86_64,$(LIBRARY_NAME))))
 endif
endif # ifeq ($(RDI_PLATFORM),lnx64)

# ppc64le
ifeq ($(RDI_PLATFORM),ppc64le)
 ifneq ($(debug),yes)
  RELEASE_LIBS_DIR := runtime/lib/ppc64le
  RELEASE_LIBS := $(LIBRARY_NAME)
  RELEASE_TYPE := internal nostrip
  include $(RDI_MAKEROOT)/release.mk

  # Release to platforms sw/lib dir
  $(foreach dir,$(RUNTIME_XCLNG_RELEASE_DIRS),\
   $(eval $(call XCLNG_release_libs_template,$(dir)/sw/lib/ppc64le,$(LIBRARY_NAME))))

  $(foreach dir,$(RUNTIME_XCLNG_INTERNAL_RELEASE_DIRS),\
   $(eval $(call XCLNG_internal_release_libs_template,$(dir)/sw/lib/ppc64le,$(LIBRARY_NAME))))
 endif
endif # ifeq ($(RDI_PLATFORM),ppc64le)

# aarch64
ifeq ($(RDI_PLATFORM),aarch64)
 ifneq ($(debug),yes)
  RELEASE_LIBS_DIR := runtime/lib/aarch64
  RELEASE_LIBS := $(LIBRARY_NAME)
  RELEASE_TYPE := customer nostrip
  include $(RDI_MAKEROOT)/release.mk

  # Release to platforms sw/lib dir
  $(foreach dir,$(RUNTIME_XCLNG_RELEASE_DIRS),\
   $(eval $(call XCLNG_release_libs_template,$(dir)/sw/lib/aarch64,$(LIBRARY_NAME))))

  $(foreach dir,$(RUNTIME_XCLNG_INTERNAL_RELEASE_DIRS),\
   $(eval $(call XCLNG_internal_release_libs_template,$(dir)/sw/lib/aarch64,$(LIBRARY_NAME))))
 endif
endif # ifeq ($(RDI_PLATFORM),ppc64le)

# Release to regular install area so that LD_LIBRARY_PATH
# doesn't have to be set to point at runtime/lib/.../
RELEASE_LIBS := $(LIBRARY_NAME)
RELEASE_TYPE := customer nostrip
include $(RDI_MAKEROOT)/release.mk

include $(RDI_MAKEROOT)/bottom.mk
