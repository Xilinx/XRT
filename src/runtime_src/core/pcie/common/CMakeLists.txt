include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${DRM_INCLUDE_DIRS}
  )

file(GLOB XRT_CORE_PCIE_COMMON_FILES
  "*.h"
  "*.cpp"
  )

add_library(core_pciecommon_objects OBJECT ${XRT_CORE_PCIE_COMMON_FILES})
