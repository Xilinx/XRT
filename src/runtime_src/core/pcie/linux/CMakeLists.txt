# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.

# Linux shim is part of the base component.  It is used by both
# Alveo and NPU components.
if (NOT XRT_BASE)
  return()
endif()

add_subdirectory(plugin/xdp)

add_library(core_pcielinux_objects OBJECT
  debug.cpp
  device_linux.cpp
  pcidev.cpp
  pcidrv.cpp
  shim.cpp
  smi_pcie.cpp
  system_linux.cpp
  )

target_compile_definitions(core_pcielinux_objects
  PRIVATE
  XCLHAL_MAJOR_VER=2
  XCLHAL_MINOR_VER=1
  )

target_include_directories(core_pcielinux_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${XRT_BINARY_DIR}/gen
  )


add_library(xrt_core SHARED
  $<TARGET_OBJECTS:core_pcielinux_plugin_xdp_objects>
  $<TARGET_OBJECTS:core_pcielinux_objects>
  $<TARGET_OBJECTS:core_pciecommon_objects>
  $<TARGET_OBJECTS:core_common_objects>
  )

add_library(xrt_core_static STATIC
  $<TARGET_OBJECTS:core_pcielinux_plugin_xdp_objects>
  $<TARGET_OBJECTS:core_pcielinux_objects>
  $<TARGET_OBJECTS:core_pciecommon_objects>
  $<TARGET_OBJECTS:core_common_objects>
  )

set_target_properties(xrt_core PROPERTIES
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

# Private dependencies for fully resolved dynamic xrt_core
target_link_libraries(xrt_core
  PRIVATE
  xrt_coreutil
  pthread
  rt
  dl
  uuid
  )

# Targets linking with xrt_core_static must also link with additional
# libraries to satisfy xrt_core_static dependencies. These type of
# link dependencies are known as INTERFACE dependencies.  Here the
# boost libraries are specified by their system name so that static of
# target can pick static link libraries of boost
target_link_libraries(xrt_core_static
  INTERFACE
  xrt_coreutil_static
  uuid
  dl
  rt
  pthread
  )

# Shim for Linux is installed in the base component as it is
# used by both Alveo and NPU components.   
install(TARGETS xrt_core xrt_core_static
  EXPORT xrt-targets
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_COMPONENT} NAMELINK_COMPONENT ${XRT_BASE_DEV_COMPONENT}
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_DEV_COMPONENT}
)
