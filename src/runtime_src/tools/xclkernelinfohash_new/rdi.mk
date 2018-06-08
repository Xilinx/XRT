include $(RDI_MAKEROOT)/top.mk

OBJS_PROPAGATE := no
include $(RDI_MAKEROOT)/objs.mk

EXEC_NAME := xclkernelinfohash_new
EXEC_OBJS := $(basename $(notdir $(wildcard $(SSCURDIR)/*.cxx)))

SHLIBRARIES := \
 shared/common/rdi_common \
 shared/common/rdi_containers

STLIBRARIES := \
 products/sdx/rdi_pic_schema_static \
 products/sdx/sdx_lmx6.0_static 

MYFINALLINK := -lcrypt -Wl,--exclude-libs=libsdx_lmx6.0_static
include $(RDI_MAKEROOT)/exec.mk

# Release the executable
RELEASE_EXECS_DIR := runtime/bin
RELEASE_EXECS_EXACT := $(EXEC)
RELEASE_TYPE := customer
include $(RDI_MAKEROOT)/release.mk

include $(RDI_MAKEROOT)/bottom.mk
