# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

xrt_add_subdirectory(common)
xrt_add_subdirectory(tools)

if(NOT WIN32)
  xrt_add_subdirectory(linux)
  xrt_add_subdirectory(driver)
  if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    # Emulation flow only works on x86_64
    xrt_add_subdirectory(emulation)
    xrt_add_subdirectory(noop)
  endif()
else()
  xrt_add_subdirectory(windows)
endif(NOT WIN32)
