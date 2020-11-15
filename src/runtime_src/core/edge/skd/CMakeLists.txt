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
  PRIVATE
  xrt_core
  dl
  )

# This linking with xrt_coreutil resolves link path for indirect link
# dependencies for libxrt_core.  The link path is embedded correctly
# in libxrt_core.so but is for some reason ignored when cross
# compiling with petalinux.  Per google this is an issue others have
# faced also and has something to do with explicit setting of sysroot.
target_link_libraries(skd
  PRIVATE
  xrt_coreutil
)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

install (TARGETS skd RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
