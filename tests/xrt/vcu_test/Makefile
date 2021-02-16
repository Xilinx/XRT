ifndef XILINX_XRT
$(error XILINX_XRT is not set)
endif

XRT_PATH=${XILINX_XRT}

CFLAGS := -Wno-unused-parameter -Wall -Wextra
CLFLAGS :=

ifeq (${debug}, 1)
CFLAGS += -g
endif

CFLAGS += -I${XRT_PATH}/include
CLFLAGS += -L${XRT_PATH}/lib -lxrt_core -lxrt_coreutil -luuid

.PHONY: all clean

all: vcu_test

%.o: %.c
	gcc -c ${CFLAGS} -o $@ $^

vcu_test: main.o dummy_plugin_dec.o xrt_utils.o
	gcc $^ ${CLFLAGS} -o $@

clean:
	rm -rf vcu_test *.o
