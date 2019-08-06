include $(SRCDIR)/../ert.mk

# BSP archive is created from SDK generated bsp
# Extract to build dir in includes target
# Also update the MicroBlaze linker script when it changes
SRC := $(SRCDIR)/scheduler.cpp
OBJ := $(BLDDIR)/scheduler.o
ELF := $(BLDDIR)/sched.elf
BIN := $(BLDDIR)/sched.bin
BSP := $(BLDDIR)/bsp
RTS := $(SRCDIR)/../..

MYCFLAGS := -I$(BSP)/include -I$(RTS)
MYLFLAGS :=  -Wl,-T,$(SRCDIR)/lscript.ld

$(OBJ): $(SRC) $(BSP).extracted $(RTS)/core/include/ert.h
	$(CPP) $(MYCFLAGS) $<

$(ELF): $(OBJ)
	$(LINK) $(MYLFLAGS) -o $@ $< -L$(BSP)/lib -lxil

$(BIN): $(ELF)
	$(MB_HOME)/bin/mb-objcopy -I elf32-microblaze -O binary $< $@

.PHONY: ert
ert: $(BIN)
