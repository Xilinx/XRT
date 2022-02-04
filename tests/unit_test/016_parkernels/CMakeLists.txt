# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)
set(TESTNAME "016_parkernels")
PROJECT(${TESTNAME})

include(../../CMake/utils.cmake)

add_executable(${TESTNAME} test-cl.cpp)
target_link_libraries(${TESTNAME} PRIVATE ${xrt_xilinxopencl_LIBRARY})

if (NOT WIN32)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

if (DEFINED ENV{XCLBIN_CREATION})
  if (DEFINED ENV{XCL_EMULATION_MODE})
    xrt_create_emconfig(${PLATFORM})
  endif()

  set(XOS "")
  set(XO_TARGETS "")

  # xrt_create_xo is a macro defined in utils.cmake for generating xo file
  xrt_create_xo(
    "${CMAKE_CURRENT_SOURCE_DIR}/mmult1.cl"
    ""
    "kernel"
  )
  # xrt_create_xclbin is macro defined in utils.cmake for generating xclbin
  xrt_create_xclbin(
    "kernel"
    ""
  )
endif()

install(TARGETS ${TESTNAME}
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
