include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${DRM_INCLUDE_DIRS}
  ${CURSES_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
  )

file(GLOB XRT_PCIE_LINUX_PLUGIN_XDP_FILES
  "plugin/xdp/*.h"
  "plugin/xdp/*.cpp"
  )

file(GLOB XRT_PCIE_LINUX_FILES
  "*.h"
  "*.cpp"
  "../common/device_pcie.cpp"
  "../../common/device_core.cpp"
  )

set(CMAKE_CXX_FLAGS "-DXCLHAL_MAJOR_VER=2 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-DXCLHAL_MINOR_VER=1 ${CMAKE_CXX_FLAGS}")

add_library(core_pcielinux_plugin_xdp_objects OBJECT ${XRT_PCIE_LINUX_PLUGIN_XDP_FILES})
add_library(core_pcielinux_objects OBJECT ${XRT_PCIE_LINUX_FILES})

add_library(xrt_core SHARED ""
  $<TARGET_OBJECTS:core_pcielinux_plugin_xdp_objects>
  $<TARGET_OBJECTS:core_pcielinux_objects>
  $<TARGET_OBJECTS:core_pciecommon_objects>
  $<TARGET_OBJECTS:core_common_objects>
  )

add_library(xrt_core_static STATIC ""
  $<TARGET_OBJECTS:core_pcielinux_plugin_xdp_objects>
  $<TARGET_OBJECTS:core_pcielinux_objects>
  $<TARGET_OBJECTS:core_pciecommon_objects>
  $<TARGET_OBJECTS:core_common_objects>
  )

set_target_properties(xrt_core PROPERTIES
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(xrt_core
  xrt_coreutil
  pthread
  rt
  dl
  boost_filesystem
  boost_system
  uuid
  )

install(TARGETS xrt_core
  LIBRARY
  DESTINATION ${XRT_INSTALL_DIR}/lib)
