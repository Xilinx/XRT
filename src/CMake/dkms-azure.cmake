
set(DKMS_FILE_NAME "dkms-azure.conf")
set(DKMS_POSTINST "postinst-azure")
set(DKMS_PRERM "prerm-azure")

configure_file(
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_FILE_NAME}.in"
  "azure/${DKMS_FILE_NAME}"
  @ONLY
  )

configure_file(
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_POSTINST}.in"
  "azure/postinst"
  @ONLY
  )

configure_file(
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_PRERM}.in"
  "azure/prerm"
  @ONLY
  )
