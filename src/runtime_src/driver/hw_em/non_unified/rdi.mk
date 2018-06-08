include $(RDI_MAKEROOT)/top.mk

RELEASE_DATA_DIR := data/emulation/hw_em
RELEASE_DATA_DIRS := generic_pcie
RELEASE_TYPE := customer
include $(RDI_MAKEROOT)/release.mk

include $(RDI_MAKEROOT)/bottom.mk
