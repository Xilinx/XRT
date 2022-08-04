# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#
set(SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}")
set(PS_KERNEL_INSTALL_DIR "${XRT_INSTALL_LIB_DIR}/ps_kernels_lib")

include_directories(
  ${SRC_DIR} 
  ${SRC_DIR}/../../../include
  ${SRC_DIR}/../../../../
  ${SRC_DIR}/../../../edge/include
  )

file(GLOB SRC_FILES
  "${SRC_DIR}/*.cpp"
  )

set(CMAKE_CXX_FLAGS "-DXAIE_DEBUG -D__PS_ENABLE_AIE__ ${CMAKE_CXX_FLAGS}")
add_definitions(-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)
add_library(aie_trace_config SHARED ${SRC_FILES})

set_target_properties(aie_trace_config PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(aie_trace_config
    PRIVATE
    xrt_coreutil
    xrt_core
	xilinxopencl
	pthread
	rt
	dl
	crypt
	stdc++
	xaiengine
)

install (TARGETS aie_trace_config
  EXPORT xrt-targets
  LIBRARY DESTINATION ${PS_KERNEL_INSTALL_DIR}
)
