# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_SOURCE_DIR}/runtime_src
  )

add_subdirectory(xmaplugin)
add_subdirectory(xmaapi)

