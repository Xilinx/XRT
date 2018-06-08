EXENAME := awssak

CXX_EXT := cpp
CL_EXT := cl

AR := ar
CPP := g++

ROOT1 = $(dir $(dir $(CURDIR)))
ROOT = $(ROOT1)..

HAL_INC := -I$(ROOT) -I../../../..

CXXFLAGS += $(HAL_INC) -I../../xrt/user_aws/
#CXXFLAGS += -pthread -lfpga_mgmt
CXXFLAGS += -pthread 

ifeq ($(ec2),1)
    CXXFLAGS += -Wall -Werror -std=c++11
    HAL_LIBNAME := $(ROOT)/xrt/user_aws/libawsxcldrv.a
else
# For bare metal testing, i.e. in non EC2 environment
    CXXFLAGS += -Wall -Werror -std=c++11 -DINTERNAL_TESTING
    HAL_LIBNAME := $(ROOT)/xrt/user_aws/libawsbmgemdrv.a
endif


ifeq ($(debug),1)
	CXXFLAGS += -g -D_DEBUG
else
	CXXFLAGS += -O2 -DNDEBUG
endif

SRCS := $(wildcard *.$(CXX_EXT))
OBJS := $(patsubst %.$(CXX_EXT), %.o, $(SRCS))

-include $(OBJS:.o=.d)

#LDFLAGS := -pthread -lfpga_mgmt
LDFLAGS := -static-libstdc++ -pthread

#$(error $(ODIR))
#$(error $(DIR))
#$(error $(ROOT))

all : $(EXENAME)

%.o: %.$(CXX_EXT)
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) $(MYCXXFLAGS) -c $< -o $@
	$(CXX) $(CXXFLAGS) $(MYCFLAGS) $(MYCXXFLAGS) -c -MM $< -o $(patsubst %.o, %.d, $@)

$(EXENAME): $(OBJS) $(HAL_LIBNAME)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(HAL_LIBNAME) -lrt

clean:
	rm -rf *.o *.d $(EXENAME)

.PHONY: all

.DEFAULT_GOAL := all
