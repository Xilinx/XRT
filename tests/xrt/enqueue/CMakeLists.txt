# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2021 Xilinx, Inc. All rights reserved.
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)
PROJECT(enqueue)
set(TESTNAME "enqueue")

include(../../CMake/utils.cmake)

add_executable(enqueue enqueue.cpp)
target_link_libraries(enqueue PRIVATE ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(enqueue PRIVATE ${uuid_LIBRARY} pthread)
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

install(TARGETS enqueue
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
