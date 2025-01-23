# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
if (NOT XRT_ALVEO)
  return()
endif()

add_library(xrt_noop SHARED
  device_noop.cpp
  shim.cpp
  system_noop.cpp
  $<TARGET_OBJECTS:core_pciecommon_objects>
  $<TARGET_OBJECTS:core_common_objects>
  )

target_include_directories(xrt_noop
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  )

target_compile_definitions(xrt_noop
  PRIVATE
  XCLHAL_MAJOR_VER=2
  XCLHAL_MINOR_VER=1
  )

target_link_libraries(xrt_noop
  PRIVATE
  xrt_coreutil
  pthread
  )

set_target_properties(xrt_noop PROPERTIES
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

install(TARGETS xrt_noop
  EXPORT xrt-targets
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_COMPONENT} NAMELINK_COMPONENT ${XRT_DEV_COMPONENT}
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
)
