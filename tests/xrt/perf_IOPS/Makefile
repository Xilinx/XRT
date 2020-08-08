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
CPPLFLAGS += -L${XRT_PATH}/lib -lxrt_core -lxrt_coreutil -luuid

.PHONY: all clean

all: xrt_api_iops xcl_api_iops

%.o: %.cpp
	g++ -std=c++11 -c ${CPPFLAGS} -o $@ $^

xrt_api_iops: xrt_api_iops.o
	g++ $^ ${CPPLFLAGS} -o $@

xcl_api_iops: xcl_api_iops.o
	g++ $^ ${CPPLFLAGS} -o $@

clean:
	rm -rf *_iops *.o
