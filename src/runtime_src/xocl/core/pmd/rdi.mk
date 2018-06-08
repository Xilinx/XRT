include $(RDI_MAKEROOT)/top.mk

ifdef ENABLE_PMD
 MYINCLUDES := \
  $(RDI_SRCROOT)/products/sdx/ocl/src/include/1_2 \
  $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src \
  $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include

 ifeq ($(RDI_PLATFORM),lnx64)
  MYCFLAGS := -Wall -Werror
  MYDEFINES := -DPMD_OCL
 endif

 CPP_SUFFIX := .cpp
 include $(RDI_MAKEROOT)/objs.mk
endif

include $(RDI_MAKEROOT)/bottom.mk
