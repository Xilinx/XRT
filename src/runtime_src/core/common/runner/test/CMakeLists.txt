# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
CMAKE_MINIMUM_REQUIRED(VERSION 3.18.0)
PROJECT(runner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if (WIN32)
  add_compile_options(/Zc:__cplusplus)
endif()

find_package(XRT REQUIRED HINTS ${XILINX_XRT}/share/cmake/XRT)
message("-- XRT_INCLUDE_DIRS=${XRT_INCLUDE_DIRS}")

add_executable(runner runner.cpp)
target_include_directories(runner PRIVATE ${XRT_INCLUDE_DIRS} ${XRT_ROOT}/src/runtime_src)
target_link_libraries(runner PRIVATE XRT::xrt_coreutil)

add_executable(recipe recipe.cpp)
target_include_directories(recipe PRIVATE ${XRT_INCLUDE_DIRS} ${XRT_ROOT}/src/runtime_src)
target_link_libraries(recipe PRIVATE XRT::xrt_coreutil)

if (NOT WIN32)
  target_link_libraries(runner PRIVATE pthread uuid dl)
  target_link_libraries(recipe PRIVATE pthread uuid dl)
endif()

install(TARGETS runner recipe)

