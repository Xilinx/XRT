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
  "../common/system_pcie.cpp"
  "../common/device_pcie.cpp"
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
  PRIVATE
  xrt_coreutil
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  pthread
  rt
  dl
  uuid
  )

install(TARGETS xrt_core
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install(TARGETS xrt_core
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)

