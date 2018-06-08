# AWS Bare-metal XRT HAL Library Makefile

CXX := g++
CXX_EXT := cpp

ifeq ($(ec2),1)
    CXXFLAGS := -Werror -std=c++11
    STLIB = libawsxcldrv.a
    SHLIB = libawsxcldrv.so
else
# For bare metal testing, i.e. in non EC2 environment
    CXXFLAGS := -Werror -std=c++11 -DINTERNAL_TESTING
    STLIB = libawsbmgemdrv.a
    SHLIB = libawsbmgemdrv.so
endif

AR := ar
ARFLAGS := rcv

LIBS := $(STLIB) $(SHLIB)

XCLHAL_VER = -DXCLHAL_MAJOR_VER=2 -DXCLHAL_MINOR_VER=1

SHIM_INC := -I../../../../ -I../../include -I../../../include -I../../../../../linux_kernel_drivers 
SHIM_INC += -I../user_common
CXXFLAGS += $(CXXFLAGS) $(XCLHAL_VER) $(SHIM_INC) -fpic -fvisibility=hidden -lrt  -lfpga_mgmt

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
	rm -rf *.o *.d ../user_common/*.o ../user_common/*.d $(LIBS)

%.o: %.$(CXX_EXT)
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) -c $< -o $@
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) -c -MM $< -o $(patsubst %.o, %.d, $@)

$(SHLIB) : $(OBJS)
	$(CXX) -shared -o $@ $^    

$(STLIB) : $(OBJS)
	$(AR) $(ARFLAGS) $@ $^ 

.PHONY: all clean

.DEFAULT_GOAL := all
