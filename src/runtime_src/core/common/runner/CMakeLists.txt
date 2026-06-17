# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024-2026 Advanced Micro Devices, Inc. All rights reserved.
add_library(core_common_runner_objects OBJECT
  capture.cpp
  repo.cpp
  runner.cpp
  replay.cpp
  cpu.cpp
  )

target_include_directories(core_common_runner_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${XRT_SOURCE_DIR}/runtime_src/core/common/elf
  )

xrt_configure_version_file(xrt-runner APP)
add_executable(xrt-runner xrt-runner-version.rc xrt-runner.cpp)
target_link_libraries(xrt-runner PRIVATE xrt_coreutil)

xrt_configure_version_file(xrt-replay APP)
add_executable(xrt-replay xrt-replay-version.rc xrt-replay.cpp)
target_link_libraries(xrt-replay PRIVATE xrt_coreutil)

xrt_configure_version_file(xrt-capture APP)
add_executable(xrt-capture xrt-capture-version.rc xrt-capture.cpp)
target_include_directories(xrt-capture PRIVATE ${XRT_SOURCE_DIR}/runtime_src)
target_link_libraries(xrt-capture PRIVATE xrt_coreutil)

if (NOT WIN32)
  target_link_libraries(xrt-replay PRIVATE pthread)
  target_link_libraries(xrt-runner PRIVATE pthread)
  target_link_libraries(xrt-capture PRIVATE pthread)
endif()

install(TARGETS xrt-replay xrt-runner xrt-capture
  RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR} COMPONENT ${XRT_BASE_COMPONENT})


