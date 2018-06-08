include $(RDI_MAKEROOT)/top.mk
include $(RDI_MAKEROOT)/subdir.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 ppc64le
include $(RDI_MAKEROOT)/toolset.mk

MYINCLUDES := \
 $(RDI_SRCROOT)/products/sdx/ocl/src/include/1_2 \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/api2 \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include

OBJS_PROPAGATE := no
CPP_SUFFIX := .cpp
include $(RDI_MAKEROOT)/objs.mk

EXEC_NAME := txocl
EXEC_OBJS := main
EXEC_EXTRA_OBJS := $(filter-out main,$(basename $(OBJS_$(SSDIR)_$(RDI_PLATFORM))))
# SHLIBRARIES := products/sdx/rdi_sdaccel_xocl
#STLIBRARIES := products/sdx/rdi_sdaccel_xocl_static
STLIBRARIES += ext/boost_unit_test_framework_static

# for now to get definition of icd
SHLIBRARIES += products/sdx/xilinxopencl

# add -lcrypt while we temporarily link with xilinxopencl
#MYFINALLINK := -lpthread -ldl
MYFINALLINK := -lpthread -ldl -lcrypt
include $(RDI_MAKEROOT)/exec.mk

RELEASE_EXECS := txocl
RELEASE_TYPE := internal
include $(RDI_MAKEROOT)/release.mk


include $(RDI_MAKEROOT)/bottom.mk
