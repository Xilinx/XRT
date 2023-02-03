ROOT = ${PWD}/../../../../../..

#INCLUDES = -I${ROOT}/src/runtime_src -I${ROOT}/src/runtime_src/core/include -I${ROOT}/build/Debug/opt/xilinx/xrt/include
#LIBRARIES = -L${ROOT}/build/Debug/opt/xilinx/xrt/lib -lxdp_core -lxrt_coreutil -L${ROOT}/build/Debug/opt/xilinx/xrt/lib/xrt/module -lxdp_device_offload_plugin

INCLUDES = -I${ROOT}/src/runtime_src -I${ROOT}/src/runtime_src/core/include -I${ROOT}/build/Release/opt/xilinx/xrt/include
LIBRARIES = -L${ROOT}/build/Release/opt/xilinx/xrt/lib -lxdp_core -lxrt_coreutil -L${ROOT}/build/Release/opt/xilinx/xrt/lib/xrt/module -lxdp_device_offload_plugin


all: trace_processor

trace_processor: main.cpp
	g++ -Wall -g ${INCLUDES} main.cpp -o trace_processor ${LIBRARIES}

clean:
	rm -rf *~ *.o trace_processor summary.csv xrt.run_summary

