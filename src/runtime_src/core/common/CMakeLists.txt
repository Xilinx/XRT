# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.
add_subdirectory(api)
add_subdirectory(xdp)

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
  $<TARGET_OBJECTS:core_common_api_library_objects>
  $<TARGET_OBJECTS:core_common_xdp_profile_objects>
  )

add_library(xrt_coreutil_static STATIC
  $<TARGET_OBJECTS:core_common_library_objects>
  $<TARGET_OBJECTS:core_common_api_library_objects>
  $<TARGET_OBJECTS:core_common_xdp_profile_objects>
  )

set_target_properties(xrt_coreutil PROPERTIES
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION}
  )

if (NOT WIN32)
  # Additional link dependencies for xrt_coreutil
  # xrt_uuid.h depends on uuid
  target_link_libraries(xrt_coreutil PRIVATE pthread dl PUBLIC uuid)

  # Targets of xrt_coreutil_static must link with these additional
  # system libraries
  target_link_libraries(xrt_coreutil_static INTERFACE uuid dl rt pthread)
endif()

install(TARGETS xrt_coreutil
  EXPORT xrt-targets
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install(TARGETS xrt_coreutil xrt_coreutil_static
  EXPORT xrt-dev-targets
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)
