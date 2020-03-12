file(GLOB XRT_CORE_EDGE_USER_AIE_FILES
  "*.h"
  "*.cpp"
  "*.c"
  )

set(CMAKE_CXX_FLAGS "-DXAIE_DEBUG ${CMAKE_CXX_FLAGS}")
add_library(core_edge_user_aie_object OBJECT ${XRT_CORE_EDGE_USER_AIE_FILES})
