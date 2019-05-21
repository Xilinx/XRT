include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${DRM_INCLUDE_DIRS}
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

add_library(xrt_core SHARED ${XRT_SRC}
  $<TARGET_OBJECTS:util>
  )

set_target_properties(xrt_core PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(xrt_core
  xrt_coreutil
  pthread
  rt
  uuid
  boost_filesystem
  boost_system
  )

install (TARGETS xrt_core LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib)
