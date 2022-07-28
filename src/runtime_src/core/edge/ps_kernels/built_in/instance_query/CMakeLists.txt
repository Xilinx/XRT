# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
set(SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}")
set(PS_KERNEL_INSTALL_DIR "${XRT_INSTALL_LIB_DIR}/ps_kernels_lib")

include_directories(
  ${SRC_DIR}
  )

file(GLOB SRC_FILES
  "${SRC_DIR}/*.cpp"
  )

set(CMAKE_CXX_FLAGS "-DXAIE_DEBUG ${CMAKE_CXX_FLAGS}")
add_definitions(-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0)
add_library(instance_query SHARED ${SRC_FILES})

set_target_properties(instance_query PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(instance_query
    PRIVATE
    xrt_coreutil
    xrt_core
)

install (TARGETS instance_query 
  EXPORT xrt-targets
  LIBRARY DESTINATION ${PS_KERNEL_INSTALL_DIR}
)
