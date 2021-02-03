ifndef XILINX_XRT
$(error XILINX_XRT is not set)
endif

XRT_PATH=${XILINX_XRT}

CPPFLAGS :=
CPPLFLAGS :=

ifeq (${debug}, 1)
CPPFLAGS += -g
endif

CPPFLAGS += -I${XRT_PATH}/include
CPPLFLAGS += -L${XRT_PATH}/lib

.PHONY: all clean

all: xrt_api_iops xcl_api_iops

%.o: %.cpp
	g++ -std=c++14 -c ${CPPFLAGS} -o $@ $^

xrt_api_iops: xrt_api_iops.o
	g++ $^ ${CPPLFLAGS} -lxrt_coreutil -luuid -o $@

xcl_api_iops: xcl_api_iops.o
	g++ $^ ${CPPLFLAGS} -lxrt_coreutil -lxrt_core -luuid -o $@

clean:
	rm -rf *_iops *.o
