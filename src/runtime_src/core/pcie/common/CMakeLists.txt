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
# Note: Would have preferrred to use list(FILTER ... to remove these files, but
#       Ubunutu 16.04 version of cmake doesn't support this, for example:
#          list(FILTER XRT_CORE_PCIE_COMMON_FILES EXCLUDE REGEX ".*device_pcie.cpp$|.*device_pcie.h$")
get_filename_component(full_path_device_pcie_cpp ${CMAKE_CURRENT_SOURCE_DIR}/device_pcie.cpp ABSOLUTE)
list(REMOVE_ITEM XRT_CORE_PCIE_COMMON_FILES "${full_path_device_pcie_cpp}")
get_filename_component(full_path_device_pcie_h ${CMAKE_CURRENT_SOURCE_DIR}/device_pcie.h ABSOLUTE)
list(REMOVE_ITEM XRT_CORE_PCIE_COMMON_FILES "${full_path_device_pcie_h}")
get_filename_component(full_path_system_pcie_cpp ${CMAKE_CURRENT_SOURCE_DIR}/system_pcie.cpp ABSOLUTE)
list(REMOVE_ITEM XRT_CORE_PCIE_COMMON_FILES "${full_path_system_pcie_cpp}")
get_filename_component(full_path_system_pcie_h ${CMAKE_CURRENT_SOURCE_DIR}/system_pcie.h ABSOLUTE)
list(REMOVE_ITEM XRT_CORE_PCIE_COMMON_FILES "${full_path_system_pcie_h}")

add_library(core_pciecommon_objects OBJECT ${XRT_CORE_PCIE_COMMON_FILES})
