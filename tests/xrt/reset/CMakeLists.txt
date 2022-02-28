# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)
PROJECT(reset)
set(TESTNAME "reset")

include(../../CMake/utils.cmake)

add_executable(xcl_reset xcl.cpp)
target_link_libraries(xcl_reset PRIVATE ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY} pthread)

add_executable(xrt_reset xrt.cpp)
target_link_libraries(xrt_reset PRIVATE ${xrt_coreutil_LIBRARY} pthread)

install(TARGETS xcl_reset xrt_reset
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})

