include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := mb32
include $(RDI_MAKEROOT)/toolset.mk

SDACCEL_RUNTIME_DIR := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src
include $(SDACCEL_RUNTIME_DIR)/ert/ert_release.mk

CPP_SUFFIX := .cpp

# BSP archive is created from SDK generated bsp
# Extract to build dir in includes target
# Also update the MicroBlaze linker script when it changes
ERT_BUILD_DIR := $(RDI_BUILD_DIR)/$(SSNAME)/$(SSDIR)
ERT_BSP_DIR := $(ERT_BUILD_DIR)/bsp
ERT_BSP_SRC := $(SSCURDIR)/sched_bsp.tar.bz2
ERT_BSP_TAR := $(ERT_BSP_DIR)/bsp.extracted
ERT_BIN_TAR := $(ERT_BUILD_DIR)/sched.bin

$(ERT_BSP_TAR) : $(ERT_BSP_SRC) | $(ERT_BSP_DIR)/.rdi
	$(TAR) -C $(dir $@) -jxf $<
	$(TOUCH) $@

ifeq ($(RDI_PLATFORM),mb32)

 ert_bsp : $(ERT_BSP_TAR)
 includes : ert_bsp

 MYINCLUDES := $(ERT_BSP_DIR)/include $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src
 include $(RDI_MAKEROOT)/objs.mk

 $(OBJS) : $(ERT_BSP_TAR)

 EXEC_NAME := sched.elf
 EXEC_OBJS := $(notdir $(basename $(OBJS)))
 MYEFLAGS := -Wl,-T,$(SSCURDIR)/lscript.ld
 MYFINALLINK := -L$(ERT_BSP_DIR)/lib -lxil
 include $(RDI_MAKEROOT)/exec.mk

 $(ERT_BIN_TAR) : $(EXEC)
	$(MB_HOME)/bin/mb-objcopy -I elf32-microblaze -O binary $< $@
 sched.bin : $(ERT_BIN_TAR)

 $(foreach dir,$(ERT_RELEASE_DIRS),\
  $(eval $(call ERT_release_execs_template,$(dir)/sw/fw,$(ERT_BIN_TAR))))

 $(foreach dir,$(ERT_INTERNAL_RELEASE_DIRS),\
  $(eval $(call ERT_internal_release_execs_template,$(dir)/sw/fw,$(ERT_BIN_TAR))))

endif
RELEASE_TYPE := customer
RELEASE_DATA_RENAME_FROM := scheduler.cpp 
RELEASE_DATA_RENAME_TO := scheduler.h 
RELEASE_DATA_DIR := data/emulation/hw_em/ip_repo/sim_embedded_scheduler_sw_v1_0/src

include $(RDI_MAKEROOT)/release.mk
include $(RDI_MAKEROOT)/bottom.mk
