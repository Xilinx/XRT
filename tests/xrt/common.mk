#lowlevel suite common makefile definitions

ifdef XILINX_SDACCEL
 XILINX_VITIS := $(XILINX_SDACCEL)
endif

ifndef XILINX_VITIS
 ifneq ($(MAKECMDGOALS),exe)
  $(error Environment variable XILINX_VITIS should point to Vitis install area)
 endif
endif

ifndef XILINX_XRT
 $(error  Environment variable XILINX_XRT should point to XRT install area)
endif

CXX := $(CROSS_COMPILE)g++
CXX_EXT := cpp
CL_EXT := cl

# Change the DSA name to point to your device
# % make DSA=mydevice ...
DSA ?= xilinx_vcu1525_dynamic_5_1
MODE ?= hw

COMMON_DIR := host_src

AR := ar
CPP := g++
VPP := $(VPPWRAP) $(XILINX_VITIS)/bin/v++

CXXFLAGS := -Wall -Werror -std=c++14
# For DSAs with 64bit addressing
CXXFLAGS += -DDSA64
CXXFLAGS += -DHAL2

ARFLAGS := rcv
CLFLAGS := --platform $(DSA) -t $(MODE)

HAL_INC := -I$(XILINX_XRT)/include
COMMON_INC := -I$(LEVEL)/$(COMMON_DIR)

CXXFLAGS += $(HAL_INC) $(COMMON_INC)
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
	cd $(ODIR); $(VPP) $(CLFLAGS) $(MYCLFLAGS) $(MYCLCFLAGS) -c -o $@ $(CURDIR)/$<

$(CL_XCLBIN): $(CL_OBJS)
	mkdir -p $(ODIR)
	cd $(ODIR); $(VPP) $(CLFLAGS) $(MYCLFLAGS) $(MYCLLFLAGS) -l -o $@ $<

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
	$(CXX) $(LDFLAGS) $(MYLDFLAGS) -o $@ $(OBJS)  -I${XILINX_XRT}/include  -L${XILINX_XRT}/lib -Wl,-rpath-link,${XILINX_XRT}/lib -lxrt_hwemu -lxrt_coreutil -ldl -luuid -pthread

endif

clean :
	rm -rf $(ODIR)

.PHONY: all xclbin exe clean lib

.DEFAULT_GOAL := all
