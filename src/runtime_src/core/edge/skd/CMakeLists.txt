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

add_dependencies(skd 
  xrt_core
  xrt_coreutil
  )

target_link_libraries(skd
  xrt_core
  xrt_coreutil
  dl
  )

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

install (TARGETS skd RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
