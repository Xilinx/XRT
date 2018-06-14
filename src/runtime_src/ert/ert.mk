RDI_DEVKITS_HOME := /tools/batonroot/rodin/devkits
MB_HOME := $(RDI_DEVKITS_HOME)/lnx64/cross/RHEL6/microblaze/lin
MB_PREFIX := mb-

CFLAGS   := -c -Wall -mlittle-endian -mxl-soft-mul -mcpu=v10.0
LFLAGS   := -mlittle-endian -Wl,--no-relax,--gc-sections

CFLAGS += -DNDEBUG -O3

C  := $(wildcard $(MB_HOME)/bin/$(MB_PREFIX)gcc)
CPP  := $(wildcard $(MB_HOME)/bin/$(MB_PREFIX)g++)
LINK := $(CPP)

ifeq ($(CPP),)
  $(error No MicroBlaze g++ compiler found)
endif
ifeq ($(C),)
  $(error No MicroBlaze gcc compiler found)
endif

CPP += $(CFLAGS) -std=c++14
C += $(CFLAGS)
LINK += $(LFLAGS)
