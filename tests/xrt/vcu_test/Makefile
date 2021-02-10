ifndef XILINX_XRT
$(error XILINX_XRT is not set)
endif

XRT_PATH=${XILINX_XRT}

CPPFLAGS :=
CPPLFLAGS :=

ifeq (${debug}, 1)
CPPFLAGS += -g
endif
CPPFLAGS += -g

CPPFLAGS += -I${XRT_PATH}/include
CPPLFLAGS += -L${XRT_PATH}/lib -lxrt_core -lxrt_coreutil -luuid

.PHONY: all clean

all: vcu_test

%.o: %.cpp
	g++ -std=c++11 -c ${CPPFLAGS} -o $@ $^

vcu_test: vcu_test.o dummy_plugin_dec.o xrt_utils.o
	g++ $^ ${CPPLFLAGS} -o $@

clean:
	rm -rf user_polling *.o
