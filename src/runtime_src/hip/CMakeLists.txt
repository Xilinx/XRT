# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.

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
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
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
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install(TARGETS xrt_hip
  EXPORT xrt-dev-targets
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)

# Release headers
install (FILES
  config.h
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/hip
  COMPONENT ${XRT_DEV_COMPONENT}
)
