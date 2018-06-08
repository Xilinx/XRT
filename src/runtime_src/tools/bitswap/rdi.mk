include $(RDI_MAKEROOT)/top.mk

OBJS_PROPAGATE := no
include $(RDI_MAKEROOT)/objs.mk

# Build the bitswap executable
EXEC_NAME := bitswap
EXEC_OBJS := bitswap
include $(RDI_MAKEROOT)/exec.mk

# Release the executable
RELEASE_EXECS_DIR := runtime/bin
RELEASE_EXECS_EXACT := $(EXEC)
RELEASE_TYPE := customer
include $(RDI_MAKEROOT)/release.mk

include $(RDI_MAKEROOT)/bottom.mk