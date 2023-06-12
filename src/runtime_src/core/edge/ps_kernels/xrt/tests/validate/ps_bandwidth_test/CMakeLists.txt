# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
set(PS_KERNEL_INSTALL_DIR "${XRT_INSTALL_LIB_DIR}/ps_kernels_lib")

set(PS_KERNEL_NAME "ps_bandwidth")

add_library(${PS_KERNEL_NAME} SHARED
  "ps_bandwidth.cpp"
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

set(XCLBIN_NAME "${PS_KERNEL_NAME}.xclbin")
set(XCLBIN_TARGET "${PS_KERNEL_NAME}_xclbin")
set(PS_KERNEL_LOCATION "lib${PS_KERNEL_NAME}.so")
set(XCLBIN_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/${XCLBIN_NAME}")

add_custom_command(
  OUTPUT ${XCLBIN_LOCATION}
  COMMAND "${MY_VITIS}/bin/xclbinutil" --output ${XCLBIN_LOCATION} --add-pskernel ${PS_KERNEL_LOCATION} --force
  DEPENDS ${PS_KERNEL_NAME}
  )

add_custom_target(${XCLBIN_TARGET} ALL
  DEPENDS ${XCLBIN_LOCATION}
  )

install (FILES ${XCLBIN_LOCATION}
  DESTINATION ${PS_KERNEL_INSTALL_DIR}
  )
