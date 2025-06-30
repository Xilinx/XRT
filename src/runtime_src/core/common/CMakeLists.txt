# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
if (NOT XRT_BASE)
  return()
endif()

add_subdirectory(api)
add_subdirectory(xdp)
add_subdirectory(runner)

if(CMAKE_VERSION VERSION_LESS "3.18.0")
  message(WARNING "CMake version is less than 3.18.0, build of submodule aiebu disabled")
elseif (${CMAKE_CXX_COMPILER} MATCHES "(arm64|ARM64)")
  message(WARNING "Compiling for ARM64, build of submodule aiebu disabled")
elseif (${XRT_NATIVE_BUILD} STREQUAL "yes")
  set(AIEBU_FULL OFF)
  set(AIEBU_SUBMODULE ON)
  xrt_add_subdirectory(aiebu)
else()
  message(WARNING "Edge device, build of submodule aiebu disabled")
endif()

add_library(core_common_library_objects OBJECT
  config_reader.cpp
  debug.cpp
  debug_ip.cpp
  device.cpp
  error.cpp
  info_aie.cpp
  info_aie2.cpp
  info_memory.cpp
  info_platform.cpp
  info_telemetry.cpp
  info_vmr.cpp
  memaccess.cpp
  message.cpp
  module_loader.cpp
  query_requests.cpp
  sensor.cpp
  system.cpp
  thread.cpp
  time.cpp
  trace.cpp
  usage_metrics.cpp
  utils.cpp
  sysinfo.cpp
  xclbin_parser.cpp
  xclbin_swemu.cpp
  smi.cpp
  )

target_include_directories(core_common_library_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  )

target_compile_definitions(core_common_library_objects
  PRIVATE
  XRT_VERSION_MAJOR="${XRT_VERSION_MAJOR}"
  )

# The scheduler object files are for auto config of scheduler. These
# files reference xrt_core symbols, hence are excluded from
# xrt_corecommon shared library and instead linked explicitly into
# client (core) libraries
add_library(core_common_objects OBJECT scheduler.cpp)
target_include_directories(core_common_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  )

add_library(xrt_coreutil SHARED
  $<TARGET_OBJECTS:core_common_library_objects>
  $<TARGET_OBJECTS:core_common_runner_objects>
  $<TARGET_OBJECTS:core_common_api_library_objects>
  $<TARGET_OBJECTS:core_common_xdp_profile_objects>
  )

add_library(xrt_coreutil_static STATIC
  $<TARGET_OBJECTS:core_common_library_objects>
  $<TARGET_OBJECTS:core_common_runner_objects>
  $<TARGET_OBJECTS:core_common_api_library_objects>
  $<TARGET_OBJECTS:core_common_xdp_profile_objects>
  )

set_target_properties(xrt_coreutil PROPERTIES
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION}
  )

################################################################
# Define include directories and definitions needed by any target that
# links with xrt_coreutil
# BUILD_INTERFACE is for targets within XRT that link with xrt_coreutil
# INSTALL_INTERFACE is for external targets that link XRT::xrt_coreutil
target_include_directories(xrt_coreutil
  PUBLIC
  $<BUILD_INTERFACE:${XRT_SOURCE_DIR}/runtime_src>
  $<INSTALL_INTERFACE:${XRT_INSTALL_INCLUDE_DIR}>
  )
  
target_include_directories(xrt_coreutil_static
  PUBLIC
  $<BUILD_INTERFACE:${XRT_SOURCE_DIR}/runtime_src>
  $<INSTALL_INTERFACE:${XRT_INSTALL_INCLUDE_DIR}>
  )

target_compile_definitions(xrt_coreutil_static
  PUBLIC
  $<BUILD_INTERFACE:XRT_STATIC_BUILD>
  $<INSTALL_INTERFACE:XRT_STATIC_BUILD>
  )

# Linking with xrt_coreutil_static requires that applications link
# with the same runtime library as xrt_coreutil_static
if (WIN32)
  target_compile_options(xrt_coreutil_static
    PUBLIC
    $<INSTALL_INTERFACE:/MT$<$<CONFIG:Debug>:d>>
    )
endif()

if (NOT WIN32)
  # Additional link dependencies for xrt_coreutil
  # xrt_uuid.h depends on uuid
  target_link_libraries(xrt_coreutil PRIVATE pthread dl PUBLIC uuid)

  # Targets of xrt_coreutil_static must link with these additional
  # system libraries
  target_link_libraries(xrt_coreutil_static INTERFACE uuid dl rt pthread)
endif()
################################################################
install(TARGETS xrt_coreutil xrt_coreutil_static
  EXPORT xrt-targets
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_COMPONENT} NAMELINK_COMPONENT ${XRT_BASE_DEV_COMPONENT}
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_DEV_COMPONENT})
