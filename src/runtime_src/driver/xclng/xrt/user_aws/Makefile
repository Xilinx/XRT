# AWS Bare-metal XRT HAL Library Makefile

CXX := g++
CXX_EXT := cpp
AR := ar
ARFLAGS := rcv

ifeq ($(ec2),1)
    CXXFLAGS := -Werror -std=c++11
    STLIB = libxrt-aws.a
    SHLIB = libxrt-aws.so
    SHIM_INC := -I../../../../ -I../../include -I../../../include -I../../../../../linux_kernel_drivers 
else
# For bare metal testing, i.e. in non EC2 environment
    CXXFLAGS := -Werror -std=c++11 -DINTERNAL_TESTING
    STLIB = libxrtbm-aws.a
    SHLIB = libxrtbm-aws.so
    SHIM_INC := -I../../../../ -I../../include -I../../../include -I../user_common -I../../../../driver/xclng/include/drm -I../../../../driver/aws/kernel/include
endif

LIBS := $(STLIB) $(SHLIB)

XCLHAL_VER = -DXCLHAL_MAJOR_VER=2 -DXCLHAL_MINOR_VER=1

CXXFLAGS += $(CXXFLAGS) $(XCLHAL_VER) $(SHIM_INC) -fpic -fvisibility=hidden -lrt  -lfpga_mgmt -Wall

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
