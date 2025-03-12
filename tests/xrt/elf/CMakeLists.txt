# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
CMAKE_MINIMUM_REQUIRED(VERSION 3.16.0)
PROJECT(elf)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)

if (MSVC)
  add_compile_options(/Zc:__cplusplus)
endif()

find_package(XRT REQUIRED HINTS ${XILINX_XRT}/share/cmake/XRT)
message("-- XRT_INCLUDE_DIRS=${XRT_INCLUDE_DIRS}")

add_executable(elf main.cpp)
target_include_directories(elf PRIVATE ${XRT_INCLUDE_DIRS})
target_link_libraries(elf PRIVATE XRT::xrt_coreutil)

install(TARGETS elf)
