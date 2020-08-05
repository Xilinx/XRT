include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
  )

file(GLOB XRT_PCIE_WINDOWS_FILES
  "*.h"
  "*.cpp"
  "../common/system_pcie.cpp"
  "../common/device_pcie.cpp"
  )

file(GLOB XRT_PCIE_WINDOWS_STATIC_FILES
  "device_windows.*"
  "../common/system_pcie.cpp"
  "../common/device_pcie.cpp"
  )

file(GLOB XRT_PCIE_WINDOWS_SHARED_FILES
  "shim.cpp"
  "mgmt.cpp"
  "perf.cpp"
  "system_windows.cpp"
  "../common/system_pcie.cpp"
  "device_windows.cpp"
  "../common/device_pcie.cpp"
  )

set(CMAKE_CXX_FLAGS "-DXCLHAL_MAJOR_VER=2 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-DXCLHAL_MINOR_VER=1 ${CMAKE_CXX_FLAGS}")

add_library(xrt_core SHARED
  ${XRT_PCIE_WINDOWS_SHARED_FILES}
  )
add_library(xrt_core_static STATIC
  ${XRT_PCIE_WINDOWS_STATIC_FILES}
  )

target_link_libraries(xrt_core
  xrt_coreutil
  )

# For DLL platforms the DLL part of a shared library is treated as a
# RUNTIME target and the corresponding import library is treated as an
# ARCHIVE target. All Windows-based systems including Cygwin are DLL
# platforms.
install(TARGETS xrt_core
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} NAMELINK_SKIP
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install(TARGETS xrt_core
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} NAMELINK_ONLY
)
