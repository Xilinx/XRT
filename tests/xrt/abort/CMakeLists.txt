# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)
PROJECT(abort)
set(TESTNAME "abort")

include(../../CMake/utils.cmake)

add_executable(abort main.cpp)
target_link_libraries(abort PRIVATE ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(abort PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

if (DEFINED ENV{XCLBIN_CREATION})
  if (DEFINED ENV{XCL_EMULATION_MODE})
    xrt_create_emconfig(${PLATFORM})
  endif()

  set(XOS "")
  set(XO_TARGETS "")

  # xrt_create_xo is a macro defined in utils.cmake for generating xo file
  xrt_create_xo(
    "${CMAKE_CURRENT_SOURCE_DIR}/ksrc/loop_vadd.cpp"
    "--hls.pre_tcl;${CMAKE_CURRENT_SOURCE_DIR}/ksrc/runPre.tcl;-k;loop_vadd"
    "kernel"
  )
  # xrt_create_xclbin is macro defined in utils.cmake for generating xclbin
  xrt_create_xclbin(
    "kernel"
    ""
  )
endif()

install(TARGETS abort
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
