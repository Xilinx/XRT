include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64
include $(RDI_MAKEROOT)/toolset.mk

ifndef $(SSID)/rdi.mk
 $(SSID)/rdi.mk := defined 
 EXPORT_HEADERS := memorymanager.h unix_socket.h
 EXPORT_HEADER_DIR := ocl/runtime_src/driver/common_em
 include $(RDI_MAKEROOT)/includes.mk
endif

EXPORT_HEADER_DIR := ocl/runtime_src/driver/common_em
include $(RDI_MAKEROOT)/proto.mk

MYINCLUDES := \
  $(RDI_INCLUDE_DIR)/ocl/runtime_src/driver/common_em \
  $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src/driver/include

OBJS_PROPAGATE := no
include $(RDI_MAKEROOT)/objs.mk
LIBRARY_NAME := rdi_common_em_static
include $(RDI_MAKEROOT)/stlib.mk

include $(RDI_MAKEROOT)/bottom.mk
