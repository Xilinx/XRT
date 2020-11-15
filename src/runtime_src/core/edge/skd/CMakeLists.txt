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

# linking with coreutil to resolve private link dependency
# of xrt_core while linking to xrt_core from its build folder
target_link_libraries(skd
  PRIVATE
  xrt_core
  xrt_coreutil
  dl
  )

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

install (TARGETS skd RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
