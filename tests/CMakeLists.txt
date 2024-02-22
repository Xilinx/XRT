# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#

CMAKE_MINIMUM_REQUIRED(VERSION 3.5.0)
PROJECT(TESTS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(unit_test)
add_subdirectory(xrt)

if (XRT_ENABLE_HIP)
  add_subdirectory(hip)
endif()
