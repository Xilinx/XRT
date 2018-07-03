include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../xrt/user_gem
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  )

file(GLOB XBSAK_FILES
  "*.h"
  "*.cpp"
  )

set(XBSAK_SRC ${XBSAK_FILES})

add_executable(xbsak ${XBSAK_SRC})

target_link_libraries(xbsak
  xrtcorestatic
  pthread
  rt
  ${CURSES_LIBRARIES}
  )

install (TARGETS xbsak RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
