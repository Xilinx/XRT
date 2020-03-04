include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  )

file(GLOB XRT_SKD_FILES
  "*.h"
  "*.cpp"
  "*.c"
  )

set(SKD_SRC
  ${XRT_SKD_FILES}
  )

add_executable(skd ${SKD_SRC})

target_link_libraries(skd
  xrt_core
  dl
  )

install (TARGETS skd RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
