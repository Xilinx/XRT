
set(DKMS_FILE_NAME "dkms-container.conf")
set(DKMS_POSTINST "postinst-container")
set(DKMS_PRERM "prerm-container")

configure_file(
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_FILE_NAME}.in"
  "container/${DKMS_FILE_NAME}"
  @ONLY
  )

configure_file(
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_POSTINST}.in"
  "container/postinst"
  @ONLY
  )

configure_file(
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_PRERM}.in"
  "container/prerm"
  @ONLY
  )
