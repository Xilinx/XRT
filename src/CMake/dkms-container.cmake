# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#

set(DKMS_FILE_NAME "dkms-container.conf")
set(DKMS_POSTINST "postinst-container")
set(DKMS_PRERM "prerm-container")

configure_file(
  "${XRT_SOURCE_DIR}/CMake/config/${DKMS_FILE_NAME}.in"
  "container/${DKMS_FILE_NAME}"
  @ONLY
  )

configure_file(
  "${XRT_SOURCE_DIR}/CMake/config/${DKMS_POSTINST}.in"
  "container/postinst"
  @ONLY
  )

configure_file(
  "${XRT_SOURCE_DIR}/CMake/config/${DKMS_PRERM}.in"
  "container/prerm"
  @ONLY
  )
