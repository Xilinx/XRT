include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${DRM_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )

file(GLOB XRT_UTIL_FILES
  "*.h"
  "*.c"
  "*.cpp"
  )

set(XRT_CORE_UTIL_SRC
  ${XRT_UTIL_FILES}
  )

add_library(util OBJECT ${XRT_CORE_UTIL_SRC})
