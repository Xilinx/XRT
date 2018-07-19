SET (ICD_FILE_NAME "xilinx.icd")

message("-- Preparing OpenCL ICD ${ICD_FILE_NAME}")

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/${ICD_FILE_NAME}.in"
  ${ICD_FILE_NAME}
  )

set(OCL_ICD_INSTALL_PREFIX "/etc/OpenCL/vendors")

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${ICD_FILE_NAME} DESTINATION ${OCL_ICD_INSTALL_PREFIX})
