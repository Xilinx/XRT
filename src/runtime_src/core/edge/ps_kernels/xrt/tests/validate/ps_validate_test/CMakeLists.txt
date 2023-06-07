# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
set(PS_KERNEL_INSTALL_DIR "${XRT_INSTALL_LIB_DIR}/ps_kernels_lib")

set(PS_KERNEL_NAME "ps_validate")

add_library(${PS_KERNEL_NAME} SHARED
  "ps_validate.cpp"
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

install (TARGETS ${PS_KERNEL_NAME}
  EXPORT xrt-targets
  LIBRARY DESTINATION ${PS_KERNEL_INSTALL_DIR}
)
