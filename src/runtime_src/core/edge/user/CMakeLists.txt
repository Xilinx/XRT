include_directories(
  ${DRM_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR} # include gen/version.h
  )

add_subdirectory(plugin/xdp)

if (DEFINED XRT_AIE_BUILD)
  add_subdirectory(aie)
endif()

file(GLOB XRT_EDGE_USER_FILES "*.cpp")

set(CMAKE_CXX_FLAGS "-DXCLHAL_MAJOR_VER=2 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-DXCLHAL_MINOR_VER=1 ${CMAKE_CXX_FLAGS}")

if (DEFINED XRT_AIE_BUILD)
  add_library(xrt_core SHARED ${XRT_EDGE_USER_FILES}
    $<TARGET_OBJECTS:core_edgeuser_plugin_xdp_objects>
    $<TARGET_OBJECTS:core_common_objects>
    $<TARGET_OBJECTS:core_edge_user_aie_object>
    $<TARGET_OBJECTS:core_edge_common_objects>
    )
else()
  add_library(xrt_core SHARED ${XRT_EDGE_USER_FILES}
    $<TARGET_OBJECTS:core_edgeuser_plugin_xdp_objects>
    $<TARGET_OBJECTS:core_common_objects>
    $<TARGET_OBJECTS:core_edge_common_objects>
    )
endif()

add_library(xrt_core_static STATIC ${XRT_EDGE_USER_FILES}
  $<TARGET_OBJECTS:core_edgeuser_plugin_xdp_no_dl_load_objects>
  $<TARGET_OBJECTS:core_edge_common_objects>
  $<TARGET_OBJECTS:core_common_objects>
  )

set_target_properties(xrt_core PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

if (DEFINED XRT_AIE_BUILD)
  target_link_libraries(xrt_core
    xrt_coreutil
    pthread
    rt
    dl
    uuid
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    metal
    xaiengine
    )
else()
  target_link_libraries(xrt_core
    xrt_coreutil
    pthread
    rt
    dl
    uuid
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    )
endif()

install (TARGETS xrt_core LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR})
