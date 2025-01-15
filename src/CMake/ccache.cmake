# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
if (XRT_CCACHE)
  
  find_program(CCACHE_PROGRAM ccache)

  if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")

    # Disable abi compile time checks which renders ccache close to useless
    message ("***** CCACHE: DISABLING ABI VERSION CHECK ******")
    add_compile_options("-DDISABLE_ABI_CHECK")
  else()
    message ("***** ccache program not found, ignoring -ccache")
  endif()

endif()
