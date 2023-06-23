# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
set(PS_KERNEL_INSTALL_DIR "${XRT_INSTALL_LIB_DIR}/ps_kernels_lib")

function(generate_ps_kernel_xclbin PS_KERNEL_NAME)
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
endfunction()

if (DEFINED MY_VITIS)
  if (DEFINED XRT_AIE_BUILD)
    add_subdirectory(sample)
    add_subdirectory(profiling)
  endif()

  add_subdirectory(xrt)
endif()
