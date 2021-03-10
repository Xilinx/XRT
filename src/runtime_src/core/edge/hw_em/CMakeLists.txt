set(EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}")
set(COMMON_EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../user")
set(AIE_HW_EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../user/aie")

include_directories(
  ${EM_SRC_DIR}
  ${DRM_INCLUDE_DIRS}
  ${COMMON_EM_SRC_DIR}
  ${BOOST_FILESYSTEM_INCLUDE_DIRS}
  ${BOOST_SYSTEM_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR} # includes version.h
  )


file(GLOB EM_SRC_FILES
  "${COMMON_EM_SRC_DIR}/*.h"
  "${COMMON_EM_SRC_DIR}/*.cpp"
  "${EM_SRC_DIR}/*.h"
  "${EM_SRC_DIR}/*.cpp"
  )

file(GLOB XRT_CORE_EDGE_USER_AIE_HW_EM_FILES
  "${AIE_HW_EM_SRC_DIR}/*.h"
  "${AIE_HW_EM_SRC_DIR}/*.cpp"
  "${AIE_HW_EM_SRC_DIR}/*.c"
  )

if (DEFINED XRT_AIE_BUILD)
  set(CMAKE_CXX_FLAGS "-DXAIE_DEBUG -D__HWEM__ ${CMAKE_CXX_FLAGS}")
  add_library(core_edge_user_aie_object_hw_em OBJECT ${XRT_CORE_EDGE_USER_AIE_HW_EM_FILES})
endif()

add_definitions(-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0 -D__HWEM__)

if (DEFINED XRT_AIE_BUILD)
  add_library(xrt_hwemu SHARED ${EM_SRC_FILES}
    $<TARGET_OBJECTS:core_common_objects>
    $<TARGET_OBJECTS:core_edge_user_aie_object_hw_em>
    $<TARGET_OBJECTS:core_edge_common_objects>
    )
else()
add_library(xrt_hwemu SHARED ${EM_SRC_FILES}
  $<TARGET_OBJECTS:core_common_objects>
  $<TARGET_OBJECTS:core_edge_common_objects>
  )
endif()

set_target_properties(xrt_hwemu PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

if (DEFINED XRT_AIE_BUILD)
  target_link_libraries(xrt_hwemu
    PRIVATE
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
  target_link_libraries(xrt_hwemu
    PRIVATE
    xrt_coreutil
    pthread
    rt
    dl
    uuid
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
  )
endif()

install (TARGETS xrt_hwemu LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR})
