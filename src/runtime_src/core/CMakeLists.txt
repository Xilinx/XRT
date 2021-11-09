# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  set(XRT_AIE_BUILD "yes")
  set(XRT_ENABLE_AIE "yes")
  add_definitions (-DXRT_ENABLE_AIE -DXRT_AIE_BUILD)
endif()

add_subdirectory(include)

if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_subdirectory(common)
  add_subdirectory(pcie)
  add_subdirectory(tools)
else()
  add_subdirectory(common)
  add_subdirectory(edge)
  add_subdirectory(tools)
endif()

if (${XRT_CLANGTIDY_REVIEW})
  add_subdirectory(edge)
endif()
  
