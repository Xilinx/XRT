# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
set(PS_KERNEL_INSTALL_DIR "${XRT_INSTALL_LIB_DIR}/ps_kernels_lib")

set(CMAKE_CXX_FLAGS "-DXAIE_DEBUG ${CMAKE_CXX_FLAGS}")
add_library(instance_query OBJECT
  instance_query.c
  )

set_target_properties(instance_query PROPERTIES 
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION}
  XCLHAL_MAJOR_VER 1
  XCLHAL_MINOR_VER 0)

target_link_libraries(instance_query
    PRIVATE
    xrt_coreutil
    xrt_core
)

install (TARGETS instance_query 
  EXPORT xrt-targets
  LIBRARY DESTINATION ${PS_KERNEL_INSTALL_DIR}
)
