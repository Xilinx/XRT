# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.
#
set(TESTNAME "ps_aie.exe")

set(xrt_core_LIBRARY xrt_core)
set(xrt_coreutil_LIBRARY xrt_coreutil)

include_directories(
  ../../../src/runtime_src/core/include src/
  ../common/includes/cmdparser
  ../common/includes/logger
  )

add_executable(${TESTNAME}
  ../common/includes/cmdparser/cmdlineparser.cpp
  ../common/includes/logger/logger.cpp src/host.cpp
  )

target_link_libraries(${TESTNAME} PRIVATE ${xrt_coreutil_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_SYSTEM_LIBRARY})

if (NOT WIN32)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY}  pthread ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY})
endif(NOT WIN32)

if (NOT DEFINED XRT_VALIDATE_DIR)
  set(XRT_VALIDATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/")
endif()

install(TARGETS ${TESTNAME}
  RUNTIME DESTINATION ${XRT_VALIDATE_DIR}
  ) 
