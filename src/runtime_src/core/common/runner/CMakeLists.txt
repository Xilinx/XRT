# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
add_library(core_common_runner_objects OBJECT
  runner.cpp
  cpu.cpp
  )

target_include_directories(core_common_runner_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  )

xrt_configure_version_file(xrt-runner APP)
add_executable(xrt-runner xrt-runner-version.rc main.cpp)
target_link_libraries(xrt-runner PRIVATE xrt_coreutil)

if (NOT WIN32)
  target_link_libraries(xrt-runner PRIVATE pthread)
endif()

install(TARGETS xrt-runner
  RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR} COMPONENT ${XRT_BASE_COMPONENT})


