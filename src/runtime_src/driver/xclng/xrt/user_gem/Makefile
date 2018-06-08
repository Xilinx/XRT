# XRT HAL Library Makefile

AR := ar
CXX := g++
CXX_EXT := cpp
CXXFLAGS :=  -Werror -std=c++11
ARFLAGS := rv
STLIB := libxclgemdrv.a
SHLIB := libxclgemdrv.so
XCLHAL_VER =-DXCLHAL_MAJOR_VER=2 -DXCLHAL_MINOR_VER=0 

SHIM_INC := -I../../../../ -I../../include -I../../../include
SHIM_INC += -I../user_common
CXXFLAGS += $(CXXFLAGS) $(XCLHAL_VER) $(SHIM_INC) -fpic -fvisibility=hidden -lrt

ifeq ($(debug),1)
	CXXFLAGS += -g -DDEBUG
else
	CXXFLAGS += -O2 -DNDEBUG
endif

SRCS := $(wildcard *.$(CXX_EXT))
SRCS += $(wildcard ../user_common/*.$(CXX_EXT))
OBJS := $(patsubst %.$(CXX_EXT), %.o, $(SRCS))
OBJS += $(wildcard ../user_common/*.o)

-include $(OBJS:.o=.d)

all: $(STLIB) $(SHLIB)

clean:
	rm -rf *.o *.d ../user_common/*.o ../user_common/*.d $(STLIB) $(SHLIB)

%.o: %.$(CXX_EXT)
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) -c $< -o $@
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) -c -MM $< -o $(patsubst %.o, %.d, $@)

$(SHLIB) : $(OBJS)
	$(CXX) -shared -o $@ $^

$(STLIB) : $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

.PHONY: all clean

.DEFAULT_GOAL := all
