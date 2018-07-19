SET (PKG_CONFIG_FILE_NAME "xrt.pc")

message("-- Preparing XRT pkg-config ${PKG_CONFIG_FILE_NAME}")

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/${PKG_CONFIG_FILE_NAME}.in"
  ${PKG_CONFIG_FILE_NAME}
  @ONLY
  )

set(XRT_PKG_CONFIG_DIR "/usr/lib/pkgconfig")

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${PKG_CONFIG_FILE_NAME} DESTINATION ${XRT_PKG_CONFIG_DIR})
