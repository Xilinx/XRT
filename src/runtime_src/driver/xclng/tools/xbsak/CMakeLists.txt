include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../xrt/user_gem
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  )

file(GLOB XBSAK_FILES
  "*.h"
  "*.cpp"
  )

set(XBSAK_SRC ${XBSAK_FILES})

add_executable(xbutil ${XBSAK_SRC})

target_link_libraries(xbutil
  xrtcorestatic
  pthread
  rt
  ${CURSES_LIBRARIES}
  )

install (TARGETS xbutil RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
install (PROGRAMS "xbsak.sh" DESTINATION ${XRT_INSTALL_DIR}/bin RENAME "xbsak")
