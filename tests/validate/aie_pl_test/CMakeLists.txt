# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2023 Xilinx, Inc. All rights reserved.
#
set(TESTNAME "aie_pl.exe")

set(xrt_core_LIBRARY xrt_core)
set(xrt_coreutil_LIBRARY xrt_coreutil)

include_directories(
  ../../../src/runtime_src/core/include src/
  ../common/includes/cmdparser
  ../common/includes/logger
  )

add_executable(${TESTNAME}
  ../common/includes/cmdparser/cmdlineparser.cpp
  ../common/includes/logger/logger.cpp
  src/pl_controller.cpp
  src/pl_controller_aie2.cpp
  src/host.cpp
  )

target_link_libraries(${TESTNAME} PRIVATE ${xrt_coreutil_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY})

if (NOT WIN32)
  target_link_libraries(${TESTNAME} PRIVATE ${uuid_LIBRARY}  pthread ${xrt_coreutil_LIBRARY} ${xrt_core_LIBRARY})
endif(NOT WIN32)

if (NOT DEFINED XRT_VALIDATE_DIR)
  set(XRT_VALIDATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/")
endif()

install(TARGETS ${TESTNAME}
  RUNTIME DESTINATION ${XRT_VALIDATE_DIR}
  ) 
