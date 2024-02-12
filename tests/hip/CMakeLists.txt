# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

CMAKE_MINIMUM_REQUIRED(VERSION 3.5.0)
PROJECT(HIP_TESTCASES)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(XILINX_XRT $ENV{XILINX_XRT})
set(XRT_CORE_LIBRARY xrt_core)
set(XRT_HIP_LIBRARY xrt_hip)

add_compile_options(-D__HIP_PLATFORM_AMD__)

if (MSVC)
  add_compile_options(/Zc:__cplusplus)
endif()

if (NOT DEFINED ${INSTALL_DIR})
  set(INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_SYSTEM_NAME}/${CMAKE_BUILD_TYPE}/${MODE}")
endif()

message("-- Looking for HIP include files...")
# We should find HIP cmake either in standard cmake locations or in the /opt/rocm location
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/opt/rocm/lib/cmake/hip;/opt/rocm/lib/cmake/amd_comgr")
# hip-config itself requires these other directories to find its dependencies
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};/opt/rocm/lib/cmake/hip;/opt/rocm/lib/cmake/amd_comgr;/opt/rocm/lib/cmake/hsa-runtime64")
include(hip-config)
message("-- Found at ${HIP_INCLUDE_DIR}")

if (XRT_CLANG_TIDY STREQUAL "ON")
  find_program(CLANG_TIDY "clang-tidy")
  if(NOT CLANG_TIDY)
    message(FATAL_ERROR "clang-tidy not found, cannot enable static analysis")
  else()
    message("-- Enabling clang-tidy")
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")
  endif()
endif()

include_directories(${HIP_INCLUDE_DIRS})

add_subdirectory(device)
add_subdirectory(vadd)
add_subdirectory(vadd-stream)
