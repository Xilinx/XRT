include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := ppc64le aarch64
include $(RDI_MAKEROOT)/toolset.mk

MYINCLUDES := \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include

OBJS_PROPAGATE := no
include $(RDI_MAKEROOT)/objs.mk


#
# Build the xclbincat executable
#
EXEC_NAME := xclbincat
EXEC_OBJS := \
	xclbincat \
	xclbincat1 \
	xclbincat0 \
	xclbinutil \
	xclbindata
MYEXECLFLAGS := -static-libstdc++
include $(RDI_MAKEROOT)/exec.mk

# Release the executable
ifeq ($(RDI_PLATFORM),lnx64)
  RELEASE_EXECS_DIR := runtime/bin
  RELEASE_EXECS_EXACT := $(EXEC)
  RELEASE_TYPE := customer
  include $(RDI_MAKEROOT)/release.mk
endif # ifeq ($(RDI_PLATFORM),lnx64)

ifeq ($(RDI_PLATFORM),ppc64le)
  RELEASE_EXECS_DIR := runtime/bin/$(RDI_PLATFORM)
  RELEASE_EXECS_EXACT := $(EXEC)
  RELEASE_TYPE := internal nostrip
  include $(RDI_MAKEROOT)/release.mk
endif # ifeq ($(RDI_PLATFORM),ppc64le)

ifeq ($(RDI_PLATFORM),aarch64)
  RELEASE_EXECS_DIR := runtime/bin/$(RDI_PLATFORM)
  RELEASE_EXECS_EXACT := $(EXEC)
  RELEASE_TYPE := customer nostrip
  include $(RDI_MAKEROOT)/release.mk
endif # ifeq ($(RDI_PLATFORM),ppc64le)


#
# Build the xclbinsplit executable
#
EXEC_NAME := xclbinsplit
EXEC_OBJS := \
	xclbinsplit \
	xclbinsplit1 \
	xclbinsplit0 \
	xclbinutil \
	xclbindata
MYEXECLFLAGS := -static-libstdc++
include $(RDI_MAKEROOT)/exec.mk

# Release the executable
ifeq ($(RDI_PLATFORM),lnx64)
  RELEASE_EXECS_DIR := runtime/bin
  RELEASE_EXECS_EXACT := $(EXEC)
  RELEASE_TYPE := customer
  include $(RDI_MAKEROOT)/release.mk
endif # ifeq ($(RDI_PLATFORM),lnx64)

ifeq ($(RDI_PLATFORM),ppc64le)
  RELEASE_EXECS_DIR := runtime/bin/$(RDI_PLATFORM)
  RELEASE_EXECS_EXACT := $(EXEC)
  RELEASE_TYPE := internal nostrip
  include $(RDI_MAKEROOT)/release.mk
endif # ifeq ($(RDI_PLATFORM),ppc64le)

ifeq ($(RDI_PLATFORM),aarch64)
  RELEASE_EXECS_DIR := runtime/bin/$(RDI_PLATFORM)
  RELEASE_EXECS_EXACT := $(EXEC)
  RELEASE_TYPE := customer nostrip
  include $(RDI_MAKEROOT)/release.mk
endif # ifeq ($(RDI_PLATFORM),aarch64)

include $(RDI_MAKEROOT)/bottom.mk
