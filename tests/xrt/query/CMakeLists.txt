# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)
PROJECT(query)
set(TESTNAME "query")

include(../../CMake/utils.cmake)

add_executable(${TESTNAME} main.cpp)
target_link_libraries(${TESTNAME} PRIVATE ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS ${TESTNAME}
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
