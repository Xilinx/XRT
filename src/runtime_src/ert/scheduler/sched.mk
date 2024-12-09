# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.
#
include $(SRCDIR)/../ert.mk

# BSP archive is created from SDK generated bsp
# Extract to build dir in includes target
# Also update the MicroBlaze linker script when it changes
RTS := $(SRCDIR)/../..
HEADERS := $(SRCDIR)/*.h $(RTS)/core/include/xgq_impl.h $(RTS)/core/include/xrt/detail/ert.h
SRC := $(SRCDIR)/sched.c $(HEADERS)
XGQ_CTRL_SRC := $(SRCDIR)/xgq_ctrl.c $(HEADERS)
XGQ_CU_SRC := $(SRCDIR)/xgq_cu.c $(HEADERS)

OBJ := $(BLDDIR)/sched.o
XGQ_CTRL_OBJ := $(BLDDIR)/xgq_ctrl.o
XGQ_CU_OBJ := $(BLDDIR)/xgq_cu.o
ELF := $(BLDDIR)/sched.elf
BIN := $(BLDDIR)/sched.bin
BSP := $(BLDDIR)/bsp

ifndef SCHED_VERSION
 export SCHED_VERSION := 0x$(shell git rev-list -1 HEAD $(SRC) | cut -c1-8)
endif

MYCFLAGS := -ffunction-sections -fdata-sections -fno-exceptions
MYCFLAGS += -I$(BSP)/include -I$(RTS) -I$(RTS)/core/include $(DEFINES) -DERT_VERSION=$(SCHED_VERSION) -DERT_SVERSION=\"$(SCHED_VERSION)\"
MYLFLAGS := -Wl,--defsym=_HEAP_SIZE=0x0 -Wl,--gc-sections
MYLFLAGS += -Wl,-T,$(BLDDIR)/lscript.ld

$(OBJ): $(SRC) $(BSP).extracted
	$(C) $(MYCFLAGS) -c -o $@ $<

$(XGQ_CTRL_OBJ): $(XGQ_CTRL_SRC)
	$(C) $(MYCFLAGS) -c -o $@ $<

$(XGQ_CU_OBJ): $(XGQ_CU_SRC)
	$(C) $(MYCFLAGS) -c -o $@ $<

$(ELF): $(OBJ) $(XGQ_CTRL_OBJ) $(XGQ_CU_OBJ)
	$(LINK) $(MYLFLAGS) -o $@ $^ -L$(BSP)/lib -lxil

$(BIN): $(ELF)
	$(MB_HOME)/bin/mb-objcopy -I elf32-microblaze -O binary $< $@

.PHONY: ert
ert: $(BIN)
