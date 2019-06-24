include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${DRM_INCLUDE_DIRS}
  )

file(GLOB XRT_CORE_EDGE_COMMON_FILES
  "*.h"
  "*.cpp"
  )

add_library(core_edgecommon_objects OBJECT ${XRT_CORE_EDGE_COMMON_FILES})
