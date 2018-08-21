#lowlevel suite common makefile definitions

ifdef XILINX_SDX
XILINX_SDACCEL := $(XILINX_SDX)
endif

ifndef XILINX_SDACCEL
$(error Environment variable XILINX_SDACCEL should point to SDAccel install area)
else
$(info XILINX_SDACCEL = $(XILINX_SDACCEL))
endif

CROSS_COMPILE := 

CXX := $(CROSS_COMPILE)g++
CXX_EXT := cpp
CL_EXT := cl

# Change the DSA name to point to your device
# % make DSA=mydevice ...
DSA := xilinx_vcu1525_dynamic_5_1

AR := ar
CPP := g++
XOCC := $(XILINX_SDACCEL)/bin/xocc

CXXFLAGS := -Wall -std=c++0x
# For DSAs with 64bit addressing
CXXFLAGS += 

ARFLAGS := rcv
CLFLAGS := --platform $(DSA)

COMMON_INC := -I$(LEVEL)

CXXFLAGS += $(MYCXXFLAGS)

ROOT := $(dir $(CURDIR))
DIR := $(notdir $(CURDIR))

ifeq ($(debug),1)
	CXXFLAGS += -g -D_DEBUG
	ODIR := $(ROOT)build/dbg/$(DIR)
else
	CXXFLAGS += -O2 -DNDEBUG
	ODIR := $(ROOT)build/opt/$(DIR)
endif

SRCS := $(wildcard *.$(CXX_EXT) )
OBJS := $(patsubst %.$(CXX_EXT), $(ODIR)/%.o, $(SRCS))
DEPS := $(patsubst %.$(CXX_EXT), $(ODIR)/%.d, $(SRCS))

CL_SRCS := $(wildcard *.$(CL_EXT))
CL_OBJS := $(patsubst %.$(CL_EXT), $(ODIR)/%.xo, $(CL_SRCS))
CL_XCLBIN := $(firstword $(patsubst %.$(CL_EXT), $(ODIR)/%.xclbin, $(CL_SRCS)))

#$(error $(ODIR))
#$(error $(DIR))
#$(error $(ROOT))

$(ODIR)/%.d: %.$(CXX_EXT)
	mkdir -p $(ODIR)
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) -c -MM $< -MT $(patsubst %.d, %.o, $@) -o $@

-include $(DEPS)

$(OBJS): $(ODIR)/%.o : %.$(CXX_EXT)
	mkdir -p $(ODIR)
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) -c $< -o $@

$(CL_OBJS): $(ODIR)/%.xo : %.$(CL_EXT)
	mkdir -p $(ODIR)
	cd $(ODIR); $(XOCC) $(CLFLAGS) $(MYCLFLAGS) $(MYCLCFLAGS) -c -o $@ $(CURDIR)/$<

$(CL_XCLBIN): $(CL_OBJS)
	mkdir -p $(ODIR)
	cd $(ODIR); $(XOCC) $(CLFLAGS) $(MYCLFLAGS) $(MYCLLFLAGS) -l -o $@ $<

ifdef LIBNAME

$(ODIR)/$(LIBNAME): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

all : lib

lib : $(ODIR)/$(LIBNAME)

endif

ifdef EXENAME

all : exe xclbin

xclbin : $(CL_XCLBIN)

exe : $(ODIR)/$(EXENAME)

$(ODIR)/$(EXENAME): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)  -I${XILINX_XRT}/include  -L${XILINX_XRT}/lib -lxilinxopencl -lxrt_core -ldl -pthread

endif

clean :
	rm -rf $(ODIR)

.PHONY: all xclbin exe clean lib

.DEFAULT_GOAL := all
