include $(RDI_MAKEROOT)/top.mk
include $(RDI_MAKEROOT)/subdir.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 arm64 ppc64le
include $(RDI_MAKEROOT)/toolset.mk

MYINCLUDES := \
 $(RDI_SRCROOT)/products/sdx/ocl/src/include/1_2 \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include \
 $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/user \

CPP_SUFFIX := .cpp
include $(RDI_MAKEROOT)/objs.mk

include $(RDI_MAKEROOT)/bottom.mk
