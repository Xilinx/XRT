include $(RDI_MAKEROOT)/top.mk

TOOLSET_HOST := lnx64
TOOLSET_TARGETS := mb32
include $(RDI_MAKEROOT)/toolset.mk

SDACCEL_RUNTIME_DIR := $(RDI_SRCROOT)/products/sdx/ocl/src/runtime_src
include $(SDACCEL_RUNTIME_DIR)/ert/ert_release.mk

# BSP archive is created from SDK generated bsp
# Extract to build dir in includes target
# Also update the MicroBlaze linker script when it changes
ERT_BUILD_DIR := $(RDI_BUILD_DIR)/$(SSNAME)/$(SSDIR)
ERT_BSP_DIR := $(ERT_BUILD_DIR)/bsp
ERT_BSP_SRC := $(SSCURDIR)/mgmt_bsp.tar.bz2
ERT_BSP_TAR := $(ERT_BSP_DIR)/bsp.extracted
ERT_BIN_TAR := $(ERT_BUILD_DIR)/mgmt.bin

$(ERT_BSP_TAR) : $(ERT_BSP_SRC) | $(ERT_BSP_DIR)/.rdi
	$(TAR) -C $(dir $@) -jxf $<
	$(TOUCH) $@

ifeq ($(RDI_PLATFORM),mb32)

 ifeq ($(MGMT_VERSION),)
  MGMT_VERSION := $(word 2,$(shell $(RDI_P4) changes -m1 $(SSCURDIR)/board_mon.c))
 endif

 ert_bsp : $(ERT_BSP_TAR)
 includes : ert_bsp

 MYCFLAGS := -DREPO_VERSION_NUMBER=$(MGMT_VERSION)
 MYINCLUDES := $(ERT_BSP_DIR)/include
 include $(RDI_MAKEROOT)/objs.mk

 $(OBJS) : $(ERT_BSP_TAR)

 EXEC_NAME := mgmt.elf
 EXEC_OBJS := $(notdir $(basename $(OBJS)))
 MYEFLAGS := -Wl,-T,$(SSCURDIR)/lscript.ld
 MYFINALLINK := -L$(ERT_BSP_DIR)/lib -lxil
 include $(RDI_MAKEROOT)/exec.mk

 $(ERT_BIN_TAR) : $(EXEC)
	$(MB_HOME)/bin/mb-objcopy -I elf32-microblaze -O binary $< $@
 mgmt.bin : $(ERT_BIN_TAR)

 $(foreach dir,$(ERT_RELEASE_DIRS),\
  $(eval $(call ERT_release_execs_template,$(dir)/sw/fw,$(ERT_BIN_TAR))))

 $(foreach dir,$(ERT_INTERNAL_RELEASE_DIRS),\
  $(eval $(call ERT_internal_release_execs_template,$(dir)/sw/fw,$(ERT_BIN_TAR))))

endif

include $(RDI_MAKEROOT)/bottom.mk
