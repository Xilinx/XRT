# Build xilinxopencl.so for ARM

CROSS_COMPILE := aarch64-linux-gnu-
CXX := $(CROSS_COMPILE)g++

ifeq ($(CROSS_COMPILE), aarch64-linux-gnu-)
	ARCH := aarch64
else
	ARCH := arm
endif

CXXFLAGS := -Wreturn-type -std=gnu++11 -fPIC -Dxilinxopencl_EXPORTS
LDFLAGS := -L$(BOOST_LIB_DIR)
LDFLAGS += -shared -lboost_filesystem_static -lboost_system_static
LDFLAGS += -Wl,-Bsymbolic -ldl -lpthread -lcrypt -lrt

RUNTIME_SRC_DIR  = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR        = $(realpath ${RUNTIME_SRC_DIR}/../../build)
OUT_DIR         := ${BUILD_DIR}/Debug/arm_libs/$(ARCH)/xilinxopencl

ifeq ($(debug), 1)
	CXXFLAGS += -g
else
	CXXFLAGS += -O3
endif

XOCL_DIR   = ${RUNTIME_SRC_DIR}/xocl
XRT_DIR    = ${RUNTIME_SRC_DIR}/xrt
IMPL_DIR   = ${RUNTIME_SRC_DIR}/impl
XCLBIN_DIR = ${RUNTIME_SRC_DIR}/xclbin

XOCL_API_SRC = $(wildcard $(XOCL_DIR)/api/*.cpp \
			  $(XOCL_DIR)/api/detail/*.cpp \
			  $(XOCL_DIR)/api/icd/*.cpp \
			  $(XOCL_DIR)/api/khronos/*.cpp \
			  $(XOCL_DIR)/api/xlnx/*.cpp \
			  $(XOCL_DIR)/api/printf/*.cpp \
			  $(XOCL_DIR)/api/plugin/xdp/*.cpp \
		)
XOCL_API_SRC_SORTED = $(sort $(XOCL_API_SRC))
XOCL_CORE_SRC = $(wildcard $(XOCL_DIR)/core/*.cpp)
XOCL_XCLBIN_SRC = $(wildcard $(XOCL_DIR)/xclbin/*.cpp)
XOCL_SRC = $(XOCL_API_SRC_SORTED) $(XOCL_CORE_SRC) $(XOCL_XCLBIN_SRC)
XOCL_OBJS = $(patsubst $(RUNTIME_SRC_DIR)/%.cpp, $(OUT_DIR)/%.o, $(XOCL_SRC))

XRT_DEVICE_SRC = $(wildcard $(XRT_DIR)/device/*.cpp)
XRT_SCHED_SRC  = $(wildcard $(XRT_DIR)/scheduler/*.cpp)
XRT_UTIL_SRC   = $(wildcard $(XRT_DIR)/util/*.cpp)
XRT_SRC = $(XRT_DEVICE_SRC) $(XRT_SCHED_SRC) $(XRT_UTIL_SRC)
XRT_OBJS = $(patsubst $(RUNTIME_SRC_DIR)/%.cpp, $(OUT_DIR)/%.o, $(XRT_SRC))

XCLBIN_SRC = $(wildcard $(XCLBIN_DIR)/*.cpp)
XCLBIN_OBJS = $(patsubst $(RUNTIME_SRC_DIR)/%.cpp, $(OUT_DIR)/%.o, $(XCLBIN_SRC))

IMPL_SRC = $(wildcard $(IMPL_DIR)/*.cpp)
IMPL_OBJS = $(patsubst $(RUNTIME_SRC_DIR)/%.cpp, $(OUT_DIR)/%.o, $(IMPL_SRC))

ALL_OBJS = $(XOCL_OBJS) $(XRT_OBJS) $(XCLBIN_OBJS) $(IMPL_OBJS)

CXXFLAGS += -I$(RUNTIME_SRC_DIR) -I$(RUNTIME_SRC_DIR)/../include/1_2 -I$(CL_INC_DIR) -I$(RUNTIME_SRC_DIR)/driver/include -I$(XOCL_DIR)/api -I$(BOOST_INC_DIR)

$(OUT_DIR)/%.o: $(RUNTIME_SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) -c $(CXXFLAGS) -o $@ $^

all: check-boost-inc check-cl-inc check-boost-lib $(OUT_DIR)/libxilinxopencl.so 

$(OUT_DIR)/libxilinxopencl.so: $(ALL_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) 

clean:
	rm -rf $(OUT_DIR)

check-boost-inc:
ifndef BOOST_INC_DIR
	$(error BOOST_INC_DIR is undefined. This should point to BOOST header)
endif

check-boost-lib:
ifndef BOOST_LIB_DIR
	$(error BOOST_LIB_DIR is undefined. This should point to BOOST library)
endif

check-cl-inc:
ifndef CL_INC_DIR
	$(error CL_INC_DIR is undefined. This should point to where has CL/ and ocl_icd.h)
endif

