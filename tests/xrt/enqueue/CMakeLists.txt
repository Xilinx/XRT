# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2021 Xilinx, Inc. All rights reserved.
#
set(TESTNAME "enqueue")

add_executable(enqueue enqueue.cpp)
target_link_libraries(enqueue PRIVATE ${xrt_coreutil_LIBRARY})

if (NOT WIN32)
  target_link_libraries(enqueue PRIVATE ${uuid_LIBRARY} pthread)
endif(NOT WIN32)

install(TARGETS enqueue
  RUNTIME DESTINATION ${INSTALL_DIR}/${TESTNAME})
