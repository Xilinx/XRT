include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${DRM_INCLUDE_DIRS}
  )

file(GLOB XRT_EDGE_USER_PLUGIN_XDP_FILES
  "plugin/xdp/*.h"
  "plugin/xdp/*.cpp"
  )

file(GLOB XRT_USER_FILES
  "*.h"
  "*.cpp"
  "*.c"
  )

set(CMAKE_CXX_FLAGS "-DXCLHAL_MAJOR_VER=2 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-DXCLHAL_MINOR_VER=1 ${CMAKE_CXX_FLAGS}")

set(XRT_SRC
  ${XRT_USER_FILES}
  )

add_library(core_edgeuser_plugin_xdp_no_dl_load_objects OBJECT ${XRT_EDGE_USER_PLUGIN_XDP_FILES})

add_compile_options("-DXRT_LOAD_XDP_HAL_PLUGIN")
add_library(core_edgeuser_plugin_xdp_objects OBJECT ${XRT_EDGE_USER_PLUGIN_XDP_FILES})

add_library(core_edgeuser_objects OBJECT ${XRT_USER_FILES})

add_library(xrt_core SHARED ${XRT_SRC}
  $<TARGET_OBJECTS:core_edgeuser_plugin_xdp_objects>
  $<TARGET_OBJECTS:core_common_objects>
  #$<TARGET_OBJECTS:core_edgecommon_objects>
  )

add_library(xrt_core_static STATIC ""
  $<TARGET_OBJECTS:core_edgeuser_plugin_xdp_no_dl_load_objects>
  $<TARGET_OBJECTS:core_edgeuser_objects>
  #$<TARGET_OBJECTS:core_edgecommon_objects>
  $<TARGET_OBJECTS:core_common_objects>
  )

set_target_properties(xrt_core PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(xrt_core
  xrt_coreutil
  pthread
  rt
  dl
  uuid
  boost_filesystem
  boost_system
  )

install (TARGETS xrt_core LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)
