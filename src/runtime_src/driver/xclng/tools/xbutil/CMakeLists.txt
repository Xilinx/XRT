include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../xrt/user_gem
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )

file(GLOB XBUTIL_FILES
  "*.h"
  "*.cpp"
  )

set(XBUTIL_SRC ${XBUTIL_FILES})

add_executable(xbutil ${XBUTIL_SRC})

target_link_libraries(xbutil
  xrt_corestatic
  pthread
  rt
  boost_filesystem
  boost_system
  ${CURSES_LIBRARIES}
  )

install (TARGETS xbutil RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
install (PROGRAMS "xbsak.sh" DESTINATION ${XRT_INSTALL_DIR}/bin RENAME "xbsak")
