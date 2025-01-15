# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_DKMS_DRIVER_SRC_BASE_DIR
# XRT_VERSION_STRING
# LINUX_KERNEL_VERSION

set(XRT_DKMS_AWS_INSTALL_DIR "/usr/src/xrt-aws-${XRT_VERSION_STRING}")
set(XRT_DKMS_AWS_INSTALL_DRIVER_DIR "${XRT_DKMS_AWS_INSTALL_DIR}/driver")

message("-- XRT DRIVER SRC BASE DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR}")

set(DKMS_FILE_NAME "dkms.conf")
set(DKMS_POSTINST "postinst-aws")
set(DKMS_PRERM "prerm-aws")

configure_file(
  "${XRT_SOURCE_DIR}/CMake/config/dkms-awsmgmt/${DKMS_FILE_NAME}.in"
  "aws/${DKMS_FILE_NAME}"
  @ONLY
  )

configure_file(
  "${XRT_SOURCE_DIR}/CMake/config/${DKMS_POSTINST}.in"
  "aws/postinst"
  @ONLY
  )

configure_file(
  "${XRT_SOURCE_DIR}/CMake/config/${DKMS_PRERM}.in"
  "aws/prerm"
  @ONLY
  )

SET (XRT_DKMS_AWS_DRIVER_SRC_DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR}/pcie/driver/aws/kernel)
SET (XRT_DKMS_AWS_DRIVER_INCLUDE_DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR}/pcie/driver/linux)
SET (XRT_DKMS_AWS_CORE_DIR ${XRT_DKMS_DRIVER_SRC_BASE_DIR})

# srcs relative to core/pcie/driver/aws/kernel
set (XRT_DKMS_AWS_DRIVER_SRCS
  mgmt/mgmt-bit.c
  mgmt/mgmt-bit.h
  mgmt/mgmt-core.c
  mgmt/mgmt-core.h
  mgmt/mgmt-cw.c
  mgmt/mgmt-cw.h
  mgmt/mgmt-firewall.c
  mgmt/mgmt-sysfs.c
  mgmt/mgmt-thread.c
  mgmt/10-awsmgmt.rules
  mgmt/Makefile
  Makefile
  )

# includes relative to core/pcie/driver/linux
SET (XRT_DKMS_AWS_DRIVER_INCLUDES
  include/xocl_ioctl.h
  include/mgmt-reg.h
  include/mgmt-ioctl.h
  include/qdma_ioctl.h
  include/profile_ioctl.h
  include/mailbox_proto.h
  )

# includes relative to core
SET (XRT_DKMS_AWS_CORE_INCLUDES
  include/xrt/detail/ert.h
  include/xrt/detail/xclbin.h
  include/xrt/deprecated/xclerr.h
  include/xclfeatures.h
  )

foreach (DKMS_FILE ${XRT_DKMS_AWS_DRIVER_SRCS})
  get_filename_component(DKMS_DIR ${DKMS_FILE} DIRECTORY)
  install(FILES ${XRT_DKMS_AWS_DRIVER_SRC_DIR}/${DKMS_FILE}
    DESTINATION ${XRT_DKMS_AWS_INSTALL_DRIVER_DIR}/aws/${DKMS_DIR}
    COMPONENT aws)
endforeach()

foreach (DKMS_FILE ${XRT_DKMS_AWS_DRIVER_INCLUDES})
  get_filename_component(DKMS_DIR ${DKMS_FILE} DIRECTORY)
  install(FILES ${XRT_DKMS_AWS_DRIVER_INCLUDE_DIR}/${DKMS_FILE}
    DESTINATION ${XRT_DKMS_AWS_INSTALL_DRIVER_DIR}/${DKMS_DIR}
    COMPONENT aws)
endforeach()

foreach (DKMS_FILE ${XRT_DKMS_AWS_CORE_INCLUDES})
  install(FILES ${XRT_DKMS_AWS_CORE_DIR}/${DKMS_FILE}
    DESTINATION ${XRT_DKMS_AWS_INSTALL_DRIVER_DIR}/include
    COMPONENT aws)
endforeach()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/aws/${DKMS_FILE_NAME}
  DESTINATION ${XRT_DKMS_AWS_INSTALL_DIR}
  COMPONENT aws)
