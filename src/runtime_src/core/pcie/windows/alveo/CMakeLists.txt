# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
add_library(core_pciewindows_alveo_objects OBJECT
  device_windows.cpp
  mgmt.cpp
  perf.cpp
  shim.cpp
  system_windows.cpp
  )

target_compile_definitions(core_pciewindows_alveo_objects
  PRIVATE
  XCLHAL_MAJOR_VER=2
  XCLHAL_MINOR_VER=1
  )

target_include_directories(core_pciewindows_alveo_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${XRT_BINARY_DIR}
  )

add_library(xrt_core SHARED
  $<TARGET_OBJECTS:core_pciewindows_alveo_objects>
  $<TARGET_OBJECTS:core_pciecommon_objects>
  )

add_library(xrt_core_static STATIC
  $<TARGET_OBJECTS:core_pciewindows_alveo_objects>
  $<TARGET_OBJECTS:core_pciecommon_objects>
  )

target_link_libraries(xrt_core
  PRIVATE
  xrt_coreutil
  )

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(xrt_core
    PRIVATE
    setupapi
  )
endif()

# For DLL platforms the DLL part of a shared library is treated as a
# RUNTIME target and the corresponding import library is treated as an
# ARCHIVE target. All Windows-based systems including Cygwin are DLL
# platforms.
install(TARGETS xrt_core xrt_core_static
  EXPORT xrt-targets
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_COMPONENT} NAMELINK_COMPONENT ${XRT_DEV_COMPONENT}
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
)
