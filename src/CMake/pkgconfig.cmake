message("-- Preparing XRT pkg-config")

if (${LINUX_FLAVOR} MATCHES "^(Ubuntu|pynqlinux)")
  set(XRT_PKG_CONFIG_DIR "/usr/lib/pkgconfig")
elseif (${LINUX_FLAVOR} MATCHES "^(RedHat|CentOS)")
  set(XRT_PKG_CONFIG_DIR "/usr/lib64/pkgconfig")
else ()
  set(XRT_PKG_CONFIG_DIR "/usr/share/pkgconfig")
endif ()

configure_file (
  ${CMAKE_SOURCE_DIR}/CMake/config/xrt.pc.in
  xrt.pc
  @ONLY
  )
install (
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/xrt.pc
  DESTINATION
  ${XRT_PKG_CONFIG_DIR}
  )

configure_file (
  ${CMAKE_SOURCE_DIR}/CMake/config/xrt-aws.pc.in
  xrt-aws.pc
  @ONLY
  )
install (
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/xrt-aws.pc
  DESTINATION
  ${XRT_PKG_CONFIG_DIR}
  )

configure_file (
  ${CMAKE_SOURCE_DIR}/CMake/config/libxmaapi.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/libxmaapi.pc
  @ONLY
  )
install (
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/libxmaapi.pc
  DESTINATION
  ${XRT_PKG_CONFIG_DIR}
  )

configure_file (
  ${CMAKE_SOURCE_DIR}/CMake/config/libxmaplugin.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/libxmaplugin.pc
  @ONLY
  )
install (
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/libxmaplugin.pc
  DESTINATION
  ${XRT_PKG_CONFIG_DIR}
  )

configure_file (
  ${CMAKE_SOURCE_DIR}/CMake/config/libxma2api.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/libxma2api.pc
  @ONLY
  )
install (
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/libxma2api.pc
  DESTINATION
  ${XRT_PKG_CONFIG_DIR}
  )

configure_file (
  ${CMAKE_SOURCE_DIR}/CMake/config/libxma2plugin.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/libxma2plugin.pc
  @ONLY
  )
install (
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/libxma2plugin.pc
  DESTINATION
  ${XRT_PKG_CONFIG_DIR}
  )


