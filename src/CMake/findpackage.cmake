message("-- Preparing XRT find_package")

include(CMakePackageConfigHelpers)

string(TOLOWER ${PROJECT_NAME} LOWER_NAME)

file(RELATIVE_PATH REL_INCLUDE_DIR
  ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME}
  ${XRT_INSTALL_DIR}/include
  )

set(CONF_INCLUDE_DIRS "\${${PROJECT_NAME}_CMAKE_DIR}/${REL_INCLUDE_DIR}")

set(CONF_EXPORT_GROUPS xrt_core xilinxopencl xrt_swemu xrt_hwemu)

set(CONF_TARGETS xrt_core xrt_coreutil xilinxopencl xrt++ xrt_swemu xrt_hwemu)

configure_file (
  ${CMAKE_SOURCE_DIR}/CMake/config/xrt.fp.in
  ${CMAKE_CURRENT_BINARY_DIR}/${LOWER_NAME}-config.cmake
  @ONLY
  )
write_basic_package_version_file (
  ${CMAKE_CURRENT_BINARY_DIR}/${LOWER_NAME}-config-version.cmake
  VERSION ${XRT_VERSION_STRING}
  COMPATIBILITY AnyNewerVersion
  )
install (
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${LOWER_NAME}-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/${LOWER_NAME}-config-version.cmake
  DESTINATION ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME}
  COMPONENT ${XRT_DEV_COMPONENT}
  )

if(TARGET xrt_core)
install(
  EXPORT xrt_core-targets
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME})
endif()

if(TARGET xilinxopencl)
install(
  EXPORT xilinxopencl-targets
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME})
endif()

if(TARGET xrt_hwemu)
install(
  EXPORT xrt_hwemu-targets
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME})
endif()

if(TARGET xrt_swemu)
install(
  EXPORT xrt_swemu-targets
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${XRT_INSTALL_DIR}/share/cmake/${PROJECT_NAME})
endif()
