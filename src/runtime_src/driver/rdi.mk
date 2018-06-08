include $(RDI_MAKEROOT)/top.mk

SUBDIR_EXCLUDE := xcldma capi2 
include $(RDI_MAKEROOT)/subdir.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := arm64
include $(RDI_MAKEROOT)/toolset.mk

include $(RDI_MAKEROOT)/objs.mk

include $(RDI_MAKEROOT)/bottom.mk
