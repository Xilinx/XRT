# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2021 Xilinx, Inc. All rights reserved.
#
set(TESTNAME "abort")

add_executable(abort main.cpp)
target_link_libraries(abort PRIVATE ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(abort PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS abort
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
