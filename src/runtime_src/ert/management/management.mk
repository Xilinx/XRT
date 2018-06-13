include $(SRCDIR)/../ert.mk

# BSP archive is created from SDK generated bsp
# Extract to build dir in includes target
# Also update the MicroBlaze linker script when it changes
SRC := $(SRCDIR)/board_mon.c
OBJ := $(BLDDIR)/board_mon.o
ELF := $(BLDDIR)/mgmt.elf
BIN := $(BLDDIR)/mgmt.bin
BSP := $(BLDDIR)/bsp

MYCFLAGS := -I$(BSP)/include
MYLFLAGS :=  -Wl,-T,$(SRCDIR)/lscript.ld

$(OBJ) : $(SRC) $(BSP).extracted
	$(C) $(MYCFLAGS) $<

$(ELF) : $(OBJ)
	$(LINK) $(MYLFLAGS) -o $@ $< -L$(BSP)/lib -lxil

$(BIN): $(ELF)
	$(MB_HOME)/bin/mb-objcopy -I elf32-microblaze -O binary $< $@

.PHONY: ert
ert: $(BIN)
