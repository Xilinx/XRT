include $(RDI_MAKEROOT)/top.mk

# Release the OpenCL 1.2 header files
RELEASE_DATA_DIR := runtime/include/1_2
RELEASE_DATA_*DIR := 1_2
RELEASE_TYPE := customer
include $(RDI_MAKEROOT)/release.mk

include $(RDI_MAKEROOT)/bottom.mk
