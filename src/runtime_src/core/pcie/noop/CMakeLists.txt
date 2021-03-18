include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
  )

file(GLOB XRT_PCIE_NOOP_FILES
  "*.h"
  "*.cpp"
  "../common/system_pcie.cpp"
  "../common/device_pcie.cpp"
  )

file(GLOB XRT_PCIE_NOOP_SHARED_FILES
  "shim.cpp"
  "system_noop.cpp"
  "../common/system_pcie.cpp"
  "device_noop.cpp"
  "../common/device_pcie.cpp"
  )

set(CMAKE_CXX_FLAGS "-DXCLHAL_MAJOR_VER=2 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-DXCLHAL_MINOR_VER=1 ${CMAKE_CXX_FLAGS}")

add_library(xrt_noop SHARED
  ${XRT_PCIE_NOOP_SHARED_FILES}
  $<TARGET_OBJECTS:core_common_objects>
  )

set_target_properties(xrt_noop PROPERTIES VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

target_link_libraries(xrt_noop
  PRIVATE
  xrt_coreutil
  pthread
  )

install(TARGETS xrt_noop
  EXPORT xrt-targets
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} ${XRT_NAMELINK_SKIP}
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR}
)

install(TARGETS xrt_noop
  EXPORT xrt-dev-targets
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_DEV_COMPONENT} ${XRT_NAMELINK_ONLY}
)
