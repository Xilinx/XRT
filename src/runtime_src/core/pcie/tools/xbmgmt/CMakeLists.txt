include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )

file(GLOB XBMGMT_FILES
  "*.h"
  "*.cpp"
  )

set(XBMGMT_SRC ${XBMGMT_FILES})
add_executable(xbmgmt ${XBMGMT_SRC})

target_link_libraries(xbmgmt
  xrt_core_static
  xrt_coreutil_static
  pthread
  boost_filesystem
  boost_system
  uuid
  )

install (TARGETS xbmgmt RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR})
