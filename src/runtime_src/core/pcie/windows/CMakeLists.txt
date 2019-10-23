include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
  )

file(GLOB XRT_PCIE_WINDOWS_FILES
  "*.h"
  "*.cpp"
  )

file(GLOB XRT_PCIE_WINDOWS_FILES
  "*.h"
  "*.cpp"
  )

set(CMAKE_CXX_FLAGS "-DXCLHAL_MAJOR_VER=2 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-DXCLHAL_MINOR_VER=1 ${CMAKE_CXX_FLAGS}")

add_library(xrt_core SHARED ${XRT_PCIE_WINDOWS_FILES})


add_library(xrt_core_static STATIC ${XRT_PCIE_WINDOWS_FILES})
if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  target_compile_options(xrt_core_static PRIVATE /MTd)
else()
  target_compile_options(xrt_core_static PRIVATE /MT)
endif()


target_link_libraries(
  xrt_core
  xrt_coreutil
  )

# For DLL platforms the DLL part of a shared library is treated as a
# RUNTIME target and the corresponding import library is treated as an
# ARCHIVE target. All Windows-based systems including Cygwin are DLL
# platforms.
install(TARGETS xrt_core
  ARCHIVE DESTINATION ${XRT_INSTALL_DIR}/lib
  LIBRARY DESTINATION ${XRT_INSTALL_DIR}/lib
  RUNTIME DESTINATION ${XRT_INSTALL_DIR}/bin)
