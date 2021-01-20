message("-- Preparing XRT find_package")

# Provides write_basic_package_version_file
include(CMakePackageConfigHelpers)

string(TOLOWER ${PROJECT_NAME} LOWER_NAME)

# Generate xrt-config.cmake
# For use by xrt consumers (using cmake) to import xrt libraries
configure_package_config_file (
  ${CMAKE_SOURCE_DIR}/CMake/config/xrt.fp.in
  ${CMAKE_CURRENT_BINARY_DIR}/${LOWER_NAME}-config.cmake
  INSTALL_DESTINATION ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME}
  )

# Generate xrt-config-version.cmake
# Consumers my require a particular version
# This enables version checking
write_basic_package_version_file (
  ${CMAKE_CURRENT_BINARY_DIR}/${LOWER_NAME}-config-version.cmake
  VERSION ${XRT_VERSION_STRING}
  COMPATIBILITY AnyNewerVersion
  )

# Install xrt-config.cmake and xrt-config-version.cmake
install (
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${LOWER_NAME}-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/${LOWER_NAME}-config-version.cmake
  DESTINATION ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME}
  COMPONENT ${XRT_DEV_COMPONENT}
  )

# Generate and install xrt-targets.cmake
# This will generate a file that details all targets we have marked for export
# as part of the xrt-targets export group
# It will provide information such as the library file names and locations post install
install(
  EXPORT xrt-targets
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME}
  )

if(WIN32)
install(
  EXPORT xrt-targets-dev
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME}
  )
endif()

