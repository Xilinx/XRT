# XRT HAL Library Makefile

AR := ar
CXX := g++
CXX_EXT := cpp
CXXFLAGS :=  -Werror -std=c++11
ARFLAGS := rv
STLIB := libxclgemdrv.a
SHLIB := libxclgemdrv.so
XCLHAL_VER =-DXCLHAL_MAJOR_VER=2 -DXCLHAL_MINOR_VER=0 

GCC=gcc
C_EXT := c

SHIM_INC := -I../../../../ -I../../include -I../../../include
SHIM_INC += -I../user_common
CXXFLAGS += $(CXXFLAGS) $(XCLHAL_VER) $(SHIM_INC) -fpic -fvisibility=hidden -lrt
CFLAGS += $(SHIM_INC) -fpic
debug=1

ifeq ($(debug),1)
	CXXFLAGS += -g -DDEBUG
else
	CXXFLAGS += -O2 -DNDEBUG
endif

SRCS := $(wildcard *.$(CXX_EXT))
SRCS += $(wildcard ../user_common/*.$(CXX_EXT))
OBJS := $(patsubst %.$(CXX_EXT), %.o, $(SRCS))
OBJS += $(wildcard ../user_common/*.o)

C_SRCS := $(wildcard *.$(C_EXT))
C_OBJS := $(patsubst %.$(C_EXT), %.o, $(C_SRCS))

-include $(OBJS:.o=.d)

all: $(STLIB) $(SHLIB)

clean:
	rm -rf *.o *.d ../user_common/*.o ../user_common/*.d $(STLIB) $(SHLIB)

%.o: %.$(C_EXT)
	$(GCC) $(CFLAGS) -c $< -o $@

%.o: %.$(CXX_EXT) %.$(C_EXT)
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) -c $< -o $@
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) -c -MM $< -o $(patsubst %.o, %.d, $@)

$(SHLIB) : $(OBJS) $(C_OBJS)
	$(CXX) -shared -o $@ $^

$(STLIB) : $(OBJS) $(C_OBJS)
	$(AR) $(ARFLAGS) $@ $^

.PHONY: all clean

.DEFAULT_GOAL := all
