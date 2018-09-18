SET (PKG_CONFIG_FILE_NAME "xrt.pc")

message("-- Preparing XRT pkg-config ${PKG_CONFIG_FILE_NAME}")

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/${PKG_CONFIG_FILE_NAME}.in"
  ${PKG_CONFIG_FILE_NAME}
  @ONLY
  )

if (${LINUX_FLAVOR} STREQUAL Ubuntu)
  set(XRT_PKG_CONFIG_DIR "/usr/lib/pkgconfig")
elseif (${LINUX_FLAVOR} MATCHES "^(RedHat|CentOS)")
  set(XRT_PKG_CONFIG_DIR "/usr/lib64/pkgconfig")
else ()
  set(XRT_PKG_CONFIG_DIR "/usr/share/pkgconfig")
endif ()

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${PKG_CONFIG_FILE_NAME} DESTINATION ${XRT_PKG_CONFIG_DIR})

configure_file(${CMAKE_SOURCE_DIR}/CMake/config/libxmaapi.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libxmaapi.pc @ONLY)
#install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libxmaapi.pc DESTINATION ${XMA_INSTALL_DIR}/lib/pkgconfig)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libxmaapi.pc DESTINATION ${XRT_PKG_CONFIG_DIR})

configure_file(${CMAKE_SOURCE_DIR}/CMake/config/libxmaplugin.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libxmaplugin.pc @ONLY)
#install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libxmaplugin.pc DESTINATION ${XMA_INSTALL_DIR}/lib/pkgconfig)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libxmaplugin.pc DESTINATION ${XRT_PKG_CONFIG_DIR})
