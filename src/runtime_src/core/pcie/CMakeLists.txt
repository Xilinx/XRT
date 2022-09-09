# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(common)
add_subdirectory(tools)

if(NOT WIN32)
  if (NOT DEFINED EXTERNAL_SHIM)
    add_subdirectory(linux)
  endif()
  add_subdirectory(driver)
  if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    # Emulation flow only works on x86_64
    add_subdirectory(emulation)
    add_subdirectory(noop)
  endif()
else()
  if (NOT DEFINED EXTERNAL_SHIM)
    add_subdirectory(windows)
  endif()
endif(NOT WIN32)
