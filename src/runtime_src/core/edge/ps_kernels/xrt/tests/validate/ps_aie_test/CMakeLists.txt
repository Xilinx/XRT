# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
set(PS_KERNEL_NAME "ps_aie")

add_library(${PS_KERNEL_NAME} SHARED
  "ps_aie.cpp"
  )

set_target_properties(${PS_KERNEL_NAME} PROPERTIES 
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION}
  )

target_link_libraries(${PS_KERNEL_NAME}
  PRIVATE
  xrt_coreutil
  xrt_core
)

generate_ps_kernel_xclbin(${PS_KERNEL_NAME})
