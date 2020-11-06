include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )

file(GLOB XBUTIL_FILES
  "*.h"
  "*.cpp"
  )

set(XBUTIL_SRC ${XBUTIL_FILES})

add_executable(xbutil ${XBUTIL_SRC})

target_link_libraries(xbutil
  xrt_core
  xrt_coreutil
  pthread
  rt
  dl
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  uuid
  ${CURSES_LIBRARIES}
  )

install (TARGETS xbutil RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR})
