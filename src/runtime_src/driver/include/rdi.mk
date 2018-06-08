include $(RDI_MAKEROOT)/top.mk

RELEASE_DATA_DIR := runtime/driver/include
RELEASE_DATA_FILES := \
 NOTICE \
 xclbin.h \
 xclhal.h \
 xclperf.h \
 xcl_app_debug.h \
 xcl_axi_checker_codes.h \
 xclhal2.h \
 xclerr.h \
 ert.h

RELEASE_TYPE := customer
include $(RDI_MAKEROOT)/release.mk

RELEASE_TYPE := customer
RELEASE_DATA_FILES := ert.h 
RELEASE_DATA_DIR := data/emulation/hw_em/ip_repo/sim_embedded_scheduler_sw_v1_0/src

include $(RDI_MAKEROOT)/release.mk
include $(RDI_MAKEROOT)/bottom.mk
