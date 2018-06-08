include $(RDI_MAKEROOT)/top.mk

RELEASE_DATA_DIR := runtime/bin
RELEASE_DATA_FILES := install.sh rtplot xbrecover xrtdeps.sh
RELEASE_TYPE := customer
include $(RDI_MAKEROOT)/release.mk

include $(RDI_MAKEROOT)/bottom.mk
