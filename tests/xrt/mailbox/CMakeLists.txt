# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)
PROJECT(mailbox)
set(TESTNAME "mailbox")

include(../../CMake/utils.cmake)

add_executable(use_mailbox use_mailbox.cpp)
target_link_libraries(use_mailbox PRIVATE ${xrt_coreutil_LIBRARY})
add_executable(no_mailbox no_mailbox.cpp)
target_link_libraries(no_mailbox PRIVATE ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(use_mailbox PRIVATE ${uuid_LIBRARY} pthread)
  target_link_libraries(no_mailbox PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

if (DEFINED ENV{XCLBIN_CREATION})
  if (DEFINED ENV{XCL_EMULATION_MODE})
    xrt_create_emconfig(${PLATFORM})
  endif()

  set(XOS "")
  set(XO_TARGETS "")

  # xrt_create_xo is a macro defined in utils.cmake for generating xo file
  xrt_create_xo(
    "${CMAKE_CURRENT_SOURCE_DIR}/ksrc/krnl_stream_vadd.cpp"
    "-k;krnl_stream_vadd"
    "krnl_stream_vadd"
  )

  xrt_create_xo(
    "${CMAKE_CURRENT_SOURCE_DIR}/ksrc/krnl_stream_vdatamover.cpp"
    "--hls.pre_tcl;${CMAKE_CURRENT_SOURCE_DIR}/ksrc/runPre.tcl;-k;krnl_stream_vdatamover"
    "krnl_stream_vdatamover"
  )

  xrt_create_xo(
    "${CMAKE_CURRENT_SOURCE_DIR}/ksrc/krnl_stream_vmult.cpp"
    "-k;krnl_stream_vmult"
    "krnl_stream_vmult"
  )

  # xrt_create_xclbin is macro defined in utils.cmake for generating xclbin
  xrt_create_xclbin(
    "kernel"
    "--config;${CMAKE_CURRENT_SOURCE_DIR}/ksrc/krnl_stream_vadd_vmult.ini"
  )
endif()

install(TARGETS use_mailbox no_mailbox
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
