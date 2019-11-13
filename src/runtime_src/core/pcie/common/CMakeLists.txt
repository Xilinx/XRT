include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${DRM_INCLUDE_DIRS}
  )

file(GLOB XRT_CORE_PCIE_COMMON_FILES
  "*.h"
  "*.cpp"
  )

# The class device_pcie should be in the pcie/common folder.  Unfortunately,
# windows doesn't use the core_pciecommon_objects due to build issues.  So
# for now exclude these files.  
#
# Note: These files are linked in directly via the pcie/linux and 
#       pcie/windows CMakeList.txt.
list(FILTER XRT_CORE_PCIE_COMMON_FILES EXCLUDE REGEX ".*device_pcie.cpp$|.*device_pcie.h$")

add_library(core_pciecommon_objects OBJECT ${XRT_CORE_PCIE_COMMON_FILES})
