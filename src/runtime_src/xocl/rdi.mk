include $(RDI_MAKEROOT)/top.mk
include $(RDI_MAKEROOT)/subdir.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 arm64 ppc64le
include $(RDI_MAKEROOT)/toolset.mk

# Enable with icd stuff is defined in this library
# # shared library is for internal use only
# # used in unit tests for faster iterations
# LIBRARY_NAME := rdi_sdaccel_xocl
# SHLIBRARIES := \
#  products/sdx/rdi_sdaccel_xrt
# STLIBRARIES := \
#  ext/boost_filesystem_static \
#  ext/boost_system_static
# MYFINALLINK := -ldl
# include $(RDI_MAKEROOT)/shlib.mk

# RELEASE_LIBS := $(LIBRARY_NAME)
# RELEASE_TYPE := internal 
# include $(RDI_MAKEROOT)/release.mk

LIBRARY_NAME := rdi_sdaccel_xocl_static
include $(RDI_MAKEROOT)/stlib.mk

include $(RDI_MAKEROOT)/bottom.mk
