include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include/experimental
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

file(GLOB XRT_AIE_FILES
  "*.h"
  "*.cpp"
  "*.c"
  )

set(AIE_SRC
  ${XRT_AIE_FILES}
  )

set(CMAKE_CXX_FLAGS "-DXAIE_DEBUG ${CMAKE_CXX_FLAGS}")

add_library(xrt_aie_object OBJECT ${AIE_SRC})
