# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
set(TESTNAME "02_simple")

add_executable(02_simple main.cpp)
target_link_libraries(02_simple PRIVATE ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(02_simple PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS 02_simple
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
