include $(SRCDIR)/../ert.mk

# BSP archive is created from SDK generated bsp
# Extract to build dir in includes target
# Also update the MicroBlaze linker script when it changes
SRC := $(SRCDIR)/sched.c 
XGQ_CTRL_SRC := $(SRCDIR)/xgq_ctrl.c
XGQ_CU_SRC := $(SRCDIR)/xgq_cu.c

OBJ := $(BLDDIR)/sched.o
XGQ_CTRL_OBJ := $(BLDDIR)/xgq_ctrl.o
XGQ_CU_OBJ := $(BLDDIR)/xgq_cu.o
ELF := $(BLDDIR)/sched.elf
BIN := $(BLDDIR)/sched.bin
BSP := $(BLDDIR)/bsp
RTS := $(SRCDIR)/../..

ifndef SCHED_VERSION
 export SCHED_VERSION := 0x$(shell git rev-list -1 HEAD $(SRC) | cut -c1-8)
endif

MYCFLAGS := -ffunction-sections -fdata-sections -fno-exceptions
MYCFLAGS += -I$(BSP)/include -I$(RTS) -I$(RTS)/core/include $(DEFINES) -DERT_VERSION=$(SCHED_VERSION) -DERT_SVERSION=\"$(SCHED_VERSION)\"
MYLFLAGS := -Wl,--defsym=_HEAP_SIZE=0x0 -Wl,--gc-sections
MYLFLAGS += -Wl,-T,$(BLDDIR)/lscript.ld

$(OBJ): $(SRC) $(BSP).extracted $(RTS)/core/include/ert.h
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