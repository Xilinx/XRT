include $(RDI_MAKEROOT)/top.mk
include $(RDI_MAKEROOT)/subdir.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := aarch64 arm64 ppc64le
include $(RDI_MAKEROOT)/toolset.mk

LIBRARY_NAME := rdi_sdaccel_xrt
STLIBRARIES := ext/boost_filesystem_static ext/boost_system_static
MYFINALLINK := -ldl -pthread

include $(RDI_MAKEROOT)/shlib.mk

LIBRARY_NAME := rdi_sdaccel_xrt_static
include $(RDI_MAKEROOT)/stlib.mk

RELEASE_LIBS := rdi_sdaccel_xrt
# for now internal
RELEASE_TYPE := internal
include $(RDI_MAKEROOT)/release.mk

include $(RDI_MAKEROOT)/bottom.mk
