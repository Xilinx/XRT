# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
set(XCLBIN_INSTALL_DIR "${XRT_INSTALL_LIB_DIR}/xclbin_lib")
set(XCLBIN_KERNELS_INSTALL_DIR "${XRT_INSTALL_LIB_DIR}/xclbin_lib/ps_kernels")

set(PS_KERNEL_NAME "ps_validate")
set(XCLBIN_NAME "ps_validate.xclbin")

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

set(PS_KERNEL_LOCATION "lib${PS_KERNEL_NAME}.so")

add_custom_command(
  OUTPUT ${XCLBIN_NAME}
  COMMAND "xclbinutil" --output ${XCLBIN_NAME} --add-pskernel ${PS_KERNEL_LOCATION}
  DEPENDS ${PS_KERNEL_NAME} "xclbinutil"
  )

add_custom_target(xclbin ALL
  DEPENDS ${XCLBIN_NAME}
  )

install (FILES ${XCLBIN_NAME}
  DESTINATION ${XCLBIN_KERNELS_INSTALL_DIR}
  )
