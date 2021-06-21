# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
set(TESTNAME "56_xclbin")

add_executable(56_xclbin main.cpp)
target_link_libraries(56_xclbin PRIVATE ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(56_xclbin PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS 56_xclbin
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
