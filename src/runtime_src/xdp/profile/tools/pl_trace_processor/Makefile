##
## Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
##
## Licensed under the Apache License, Version 2.0 (the "License"). You may
## not use this file except in compliance with the License. A copy of the
## License is located at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
## WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
## License for the specific language governing permissions and limitations
## under the License.
##

ROOT = ${PWD}/../../../../../..

#INCLUDES = -I${ROOT}/src/runtime_src -I${ROOT}/src/runtime_src/core/include -I${ROOT}/build/Debug/opt/xilinx/xrt/include
#LIBRARIES = -L${ROOT}/build/Debug/opt/xilinx/xrt/lib -lxdp_core -lxrt_coreutil -L${ROOT}/build/Debug/opt/xilinx/xrt/lib/xrt/module -lxdp_device_offload_plugin

xrt_install_path := "/opt/xilinx/xrt"
ifdef XRT_INSTALL_PATH
	xrt_install_dir := ${XRT_INSTALL_PATH}
endif

INCLUDES = -I${ROOT}/src/runtime_src -I${ROOT}/src/runtime_src/core/include -I${ROOT}/build/Release${XRT_INSTALL_PATH}/include
LIBRARIES = -L${ROOT}/build/Release${xrt_install_dir}/lib -lxdp_core -lxrt_coreutil -L${ROOT}/build/Release${XRT_INSTALL_PATH}/lib/xrt/module -lxdp_device_offload_plugin


all: trace_processor

trace_processor: main.cpp
	g++ -Wall -g ${INCLUDES} main.cpp -o trace_processor ${LIBRARIES}

clean:
	rm -rf *~ *.o trace_processor summary.csv xrt.run_summary

