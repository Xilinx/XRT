# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)
set(TESTNAME "000_verify")
PROJECT(${TESTNAME})

find_package(XRT)

set(xrt_xilinxopencl_LIBRARY XRT::xilinxopencl)

add_executable(${TESTNAME} hello.cpp)
target_link_libraries(${TESTNAME} PRIVATE ${xrt_xilinxopencl_LIBRARY})
target_include_directories(${TESTNAME} PRIVATE ${XRT_INCLUDE_DIRS})

if (NOT WIN32)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS ${TESTNAME}
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
