# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)
PROJECT(100_ert_ncu)
set(TESTNAME "100_ert_ncu")

include(../../CMake/utils.cmake)

add_executable(xrt xrt.cpp)
target_link_libraries(xrt PRIVATE ${xrt_core_LIBRARY})

add_executable(xrtx xrtx.cpp)
target_link_libraries(xrtx PRIVATE ${xrt_coreutil_LIBRARY})

add_executable(xrtxx xrtxx.cpp)
target_link_libraries(xrtxx PRIVATE ${xrt_coreutil_LIBRARY})

add_executable(xrtxx-mt xrtxx-mt.cpp)
target_link_libraries(xrtxx-mt PRIVATE ${xrt_coreutil_LIBRARY})

add_executable(xrtxx-ip xrtxx-ip.cpp)
target_link_libraries(xrtxx-ip PRIVATE ${xrt_coreutil_LIBRARY})

add_executable(ocl ocl.cpp)
target_link_libraries(ocl PRIVATE ${xrt_xilinxopencl_LIBRARY})
if (WIN32)
  set(OCL_ROOT c:/Xilinx/XRT/ext)
  set(OpenCL_INCLUDE_DIR ${OCL_ROOT}/include)
  find_library(OpenCL_LIBRARY
    NAMES OpenCL
    HINTS "${OCL_ROOT}/lib")

  target_include_directories(ocl PUBLIC ${OpenCL_INCLUDE_DIR})
endif (WIN32)
target_compile_options(ocl PUBLIC
  "-DCL_TARGET_OPENCL_VERSION=120"
  "-DCL_HPP_MINIMUM_OPENCL_VERSION=120"
  "-DCL_HPP_TARGET_OPENCL_VERSION=120"
  "-DCL_USE_DEPRECATED_OPENCL_1_2_APIS"
  )

if (NOT WIN32)
  target_link_libraries(xrt PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(xrtx PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(xrtxx PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(xrtxx-mt PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(xrtxx-ip PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(ocl PRIVATE pthread)
endif(NOT WIN32)

if (DEFINED ENV{XCLBIN_CREATION})
  if (DEFINED ENV{XCL_EMULATION_MODE})
    xrt_create_emconfig(${PLATFORM})
  endif()

  set(XOS "")
  set(XO_TARGETS "")

  # xrt_create_xo is a macro defined in utils.cmake for generating xo file
  xrt_create_xo(
    "${CMAKE_CURRENT_SOURCE_DIR}/kernel.cl"
    ""
    "kernel"
  )
  # xrt_create_xclbin is macro defined in utils.cmake for generating xclbin
  xrt_create_xclbin(
    "kernel"
    "--config;${CMAKE_CURRENT_SOURCE_DIR}/vitis_link.cfg"
  )
endif()

install(TARGETS xrt xrtx xrtxx xrtxx-mt xrtxx-ip ocl
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})

