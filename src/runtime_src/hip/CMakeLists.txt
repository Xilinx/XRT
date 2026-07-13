# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

# config file for getting hip headers
include(hip_config.cmake)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  find_program(CLANG_TIDY "clang-tidy")
  if(NOT CLANG_TIDY)
    message(WARNING "-- clang-tidy not found, cannot enable static analysis")
  else()
    message("-- Enabling clang-tidy")
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
  endif()
endif()

add_subdirectory(core)
add_subdirectory(api)

if (WIN32)
  add_library(xrt_hip SHARED
    $<TARGET_OBJECTS:hip_core_library_objects>
    $<TARGET_OBJECTS:hip_api_library_objects>
    xrt_hip.def
  )
else()
  add_library(xrt_hip SHARED
    $<TARGET_OBJECTS:hip_core_library_objects>
    $<TARGET_OBJECTS:hip_api_library_objects>
  )
endif()

target_include_directories(xrt_hip
  PUBLIC
  $<BUILD_INTERFACE:${XRT_SOURCE_DIR}/runtime_src>
  $<INSTALL_INTERFACE:${XRT_INSTALL_INCLUDE_DIR}>
)

target_link_libraries(xrt_hip
  PRIVATE
  xrt_coreutil
)

set_target_properties(xrt_hip PROPERTIES
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION}
)

install(TARGETS xrt_hip
  EXPORT xrt-targets
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_COMPONENT} NAMELINK_COMPONENT ${XRT_BASE_DEV_COMPONENT}
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_DEV_COMPONENT}
)

# Release headers
install (FILES hip_xrt.h
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/hip
  COMPONENT ${XRT_BASE_DEV_COMPONENT}
)

# Install the vendored minimal HIP host headers so downstream consumers
# (e.g. the HIP unit tests) can build against them without a ROCm install.
# On x86_64 Linux a system ROCm/HIP is expected to provide these headers, so
# install them only on Windows (as before) and on arm64 platforms.
if (WIN32 OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
  install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/detail/hip
    DESTINATION ${XRT_INSTALL_INCLUDE_DIR}
    COMPONENT ${XRT_BASE_DEV_COMPONENT}
  )
endif()
