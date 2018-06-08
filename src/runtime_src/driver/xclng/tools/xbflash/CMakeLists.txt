include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../xrt/user_gem
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  )

file(GLOB XBFLASH_FILES
  "*.h"
  "*.cpp"
  )

set(XBFLASH_SRC ${XBFLASH_FILES})

add_executable(xbflash ${XBFLASH_SRC})

target_link_libraries(xbflash
  xrtcorestatic
  pthread
  )

install (TARGETS xbflash RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
