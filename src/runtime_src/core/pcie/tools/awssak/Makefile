EXENAME := awssak

CXX_EXT := cpp
CL_EXT := cl

AR := ar
CXX := g++

ROOT1 = $(dir $(dir $(CURDIR)))
ROOT = $(ROOT1)..
HAL_INC := -I$(ROOT) -I../../../..

CXXFLAGS := -Wall -Werror -std=c++11
CXXFLAGS += $(HAL_INC) -I../../xrt/user_aws/ -pthread

ifeq ($(ec2),1)
    AWS_HAL_LIBNAME := $(ROOT)/xrt/user_aws/libawsxcldrv.a
else # For bare metal testing, i.e. in non EC2 environment
    AWS_HAL_LIBNAME := $(ROOT)/xrt/user_aws/libxrtbm-aws.a
    CXXFLAGS += -DINTERNAL_TESTING
endif

ifeq ($(debug),1)
        CXXFLAGS += -g -D_DEBUG
else
	CXXFLAGS += -O2 -DNDEBUG
endif

SRCS := $(wildcard *.$(CXX_EXT))
OBJS := $(patsubst %.$(CXX_EXT), %.o, $(SRCS))

-include $(OBJS:.o=.d)

AWS_FPGA_MGMTLIB := fpga_mgmt
AWS_FPGA_MGMTLIB_DIR := $(SDK_DIR)/userspace/lib

ifeq ($(ec2),1)
    LDFLAGS += -L$(AWS_FPGA_MGMTLIB_DIR)
	LDLIBS += -l$(AWS_FPGA_MGMTLIB)
else
    LDFLAGS += -static-libstdc++ -pthread
endif

all : $(EXENAME)

%.o: %.$(CXX_EXT)
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) $(MYCXXFLAGS) -c $< -o $@
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) $(MYCXXFLAGS) -c -MM $< -o $(patsubst %.o, %.d, $@)

$(EXENAME): $(OBJS) $(AWS_HAL_LIBNAME)
	$(CXX) -o $@ $(OBJS) $(AWS_HAL_LIBNAME) $(LDFLAGS) $(LDLIBS) -lrt

clean:
	rm -rf *.o *.d $(EXENAME)

.PHONY: all

.DEFAULT_GOAL := all
