include $(RDI_MAKEROOT)/top.mk

RELEASE_DATA_DIR := data/emulation/cpu_em
RELEASE_DATA_DIRS := generic_pcie zynq zynqu
RELEASE_TYPE := customer
include $(RDI_MAKEROOT)/release.mk

include $(RDI_MAKEROOT)/bottom.mk
