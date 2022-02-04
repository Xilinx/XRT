# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2021 Xilinx, Inc. All rights reserved.
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)
PROJECT(m2m_arg)
set(TESTNAME "m2m_arg")

include(../../CMake/utils.cmake)

add_executable(m2m_arg main.cpp)
target_link_libraries(m2m_arg PRIVATE ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(m2m_arg PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

if (DEFINED ENV{XCLBIN_CREATION})
  if (DEFINED ENV{XCL_EMULATION_MODE})
    xrt_create_emconfig(${PLATFORM})
  endif()

  set(XOS "")
  set(XO_TARGETS "")

  # xrt_create_xo is a macro defined in utils.cmake for generating xo file
  xrt_create_xo(
    "${CMAKE_CURRENT_SOURCE_DIR}/ksrc/krnl_vadd.cpp"
    "-k;krnl_vadd"
    "krnl_vadd"
  )

  xrt_create_xo(
    "${CMAKE_CURRENT_SOURCE_DIR}/ksrc/krnl_vmult_factor.cpp"
    "-k;krnl_vmult_factor"
    "krnl_vmult_factor"
  )

  xrt_create_xo(
    "${CMAKE_CURRENT_SOURCE_DIR}/ksrc/krnl_vadd_factor.cpp"
    "-k;krnl_vadd_factor"
    "krnl_vadd_factor"
  )

  # xrt_create_xclbin is macro defined in utils.cmake for generating xclbin
  xrt_create_xclbin(
    "kernel"
    "--config;${CMAKE_CURRENT_SOURCE_DIR}/ksrc/vitis_link.cfg"
  )
endif()

install(TARGETS m2m_arg
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
