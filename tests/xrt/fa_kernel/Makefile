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

all: user_polling

%.o: %.cpp
	g++ -std=c++11 -c ${CPPFLAGS} -o $@ $^

user_polling: user_polling.o
	g++ $^ ${CPPLFLAGS} -o $@

krnl_increment_polling: krnl_increment_polling.o
	g++ $^ ${CPPLFLAGS} -o $@

clean:
	rm -rf user_polling *.o
