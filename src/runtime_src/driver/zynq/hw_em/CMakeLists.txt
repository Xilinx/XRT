set(EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}")
set(COMMON_EM_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../user")

include_directories(
  ${EM_SRC_DIR}
  ${COMMON_EM_SRC_DIR}
  ${BOOST_FILESYSTEM_INCLUDE_DIRS}
  ${BOOST_SYSTEM_INCLUDE_DIRS}
  )


file(GLOB EM_SRC_FILES
  "${COMMON_EM_SRC_DIR}/*.h"
  "${COMMON_EM_SRC_DIR}/*.cpp"
  "${EM_SRC_DIR}/*.h"
  "${EM_SRC_DIR}/*.cpp"
  )

add_definitions(-DXCLHAL_MAJOR_VER=1 -DXCLHAL_MINOR_VER=0 -D__HWEM__)
add_library(hw_emu_objects OBJECT ${EM_SRC_FILES})

set(CURR_SOURCE "")
add_library(xrt_hwemu SHARED ${CURR_SOURCE}
  $<TARGET_OBJECTS:hw_emu_objects>
  $<TARGET_OBJECTS:util>  
  )

set_target_properties(xrt_hwemu PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(xrt_hwemu
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  rt
  util
  )

install (TARGETS xrt_hwemu LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)
