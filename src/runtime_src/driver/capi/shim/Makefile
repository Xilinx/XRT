# XDMA HAL Driver Makefile

ifndef XILINX_SDACCEL
$(error Environment variable XILINX_SDACCEL should point to SDAccel install area)
else
$(info XILINX_SDACCEL = $(XILINX_SDACCEL))
endif

ARCH = $(shell uname -m)

#LIBNAME := libcapidrv.so

CXL_ROOT := /proj/xsjhdstaff2/hemn/rodinWork/HEAD/src/products/sdaccel/ext/libcxl
CXX := g++
CXX_EXT := cpp
CXXFLAGS := -Wall -Werror
XCLHAL_VER =-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=1

SHIM_INC = -I../include -I$(XILINX_SDACCEL)/runtime/driver/include -I${CXL_ROOT}/include
CXXFLAGS += $(CXXFLAGS) $(XCLHAL_VER) $(SHIM_INC) -std=c++0x -fpic -fvisibility=hidden -lrt

ifeq ($(debug),1)
	CXXFLAGS += -g -DDEBUG
else
	CXXFLAGS += -g -O2 -DNDEBUG
endif

ifeq ($(ARCH), ppc64le)
    #$(info arch is ppc64le)
    LIBNAME :=  libcapidrv.so
else 
    #$(info arch is x86_64)
    #This macro is W/A for baton
    CXXFLAGS += -D__USE_XOPEN2K8 
    LIBNAME := libcapisimdrv.so
endif

SRCS := $(wildcard *.$(CXX_EXT))
OBJS := $(patsubst %.$(CXX_EXT), %.o, $(SRCS))

-include $(OBJS:.o=.d)

all: $(LIBNAME)

clean:
	rm -rf *.o *.d $(LIBNAME)

%.o: %.$(CXX_EXT)
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) -c $< -o $@
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) -c -MM $< -o $(patsubst %.o, %.d, $@)

$(LIBNAME) : $(OBJS)
	$(CXX) -shared -o $@ $(OBJS) $(CXL_ROOT)/lib/$(ARCH)/libcxl.a

.PHONY: all clean

.DEFAULT_GOAL := all
