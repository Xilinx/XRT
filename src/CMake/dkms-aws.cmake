# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_DKMS_DRIVER_SRC_BASE_DIR

set (XRT_DKMS_INSTALL_DIR_aws "/usr/src/xrt-aws-${XRT_VERSION_STRING}")

message("-- XRT DRIVER SRC BASE DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR}")

SET (DKMS_FILE_NAME "dkms.conf")
SET (DKMS_POSTINST "postinst-aws")
SET (DKMS_PRERM "prerm-aws")

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/dkms-awsmgmt/${DKMS_FILE_NAME}.in"
  "xrt-aws-prep/${DKMS_FILE_NAME}"
  )

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_POSTINST}.in"
  "xrt-aws-prep/${DKMS_POSTINST}"
  )

configure_file (
  "${CMAKE_SOURCE_DIR}/CMake/config/${DKMS_PRERM}.in"
  "xrt-aws-prep/${DKMS_PRERM}"
  )

SET (XRT_DKMS_SRCS_aws
  driver/aws/kernel/mgmt/mgmt-bit.c
  driver/aws/kernel/mgmt/mgmt-bit.h
  driver/aws/kernel/mgmt/mgmt-core.c
  driver/aws/kernel/mgmt/mgmt-core.h
  driver/aws/kernel/mgmt/mgmt-cw.c
  driver/aws/kernel/mgmt/mgmt-cw.h
  driver/aws/kernel/mgmt/mgmt-firewall.c
  driver/aws/kernel/mgmt/mgmt-sysfs.c
  driver/aws/kernel/mgmt/mgmt-thread.c
  driver/aws/kernel/mgmt/10-awsmgmt.rules
  driver/aws/kernel/mgmt/Makefile
  driver/xclng/include/xocl_ioctl.h
  driver/xclng/include/mgmt-reg.h
  driver/xclng/include/mgmt-ioctl.h
  driver/xclng/include/qdma_ioctl.h
  #driver/xclng/include/devices.h
  driver/include/ert.h
  driver/include/xclfeatures.h
  driver/include/xclbin.h
  driver/include/xclerr.h
  )

SET (XRT_DKMS_ABS_SRCS_aws)

foreach (DKMS_FILE ${XRT_DKMS_SRCS_aws})
  get_filename_component(DKMS_DIR ${DKMS_FILE} DIRECTORY)
  install(FILES ${XRT_DKMS_DRIVER_SRC_BASE_DIR}/${DKMS_FILE} DESTINATION ${XRT_DKMS_INSTALL_DIR_aws}/${DKMS_DIR})
  list (APPEND XRT_DKMS_ABS_SRCS_aws ${XRT_DKMS_DRIVER_SRC_BASE_DIR}/${DKMS_FILE})
endforeach()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xrt-aws-prep/${DKMS_FILE_NAME} DESTINATION ${XRT_DKMS_INSTALL_DIR_aws})

