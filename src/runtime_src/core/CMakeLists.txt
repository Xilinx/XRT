# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(include)

if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  xrt_add_subdirectory(common)
  xrt_add_subdirectory(pcie)
  xrt_add_subdirectory(tools)
else()
  xrt_add_subdirectory(common)
  xrt_add_subdirectory(edge)
  xrt_add_subdirectory(tools)
endif()
