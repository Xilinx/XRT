# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
message("-- Preparing XRT pkg-config")

configure_file (
  ${XRT_SOURCE_DIR}/CMake/config/xrt.pc.in
  xrt.pc
  @ONLY
  )
install (
  FILES ${CMAKE_CURRENT_BINARY_DIR}/xrt.pc
  DESTINATION ${XRT_INSTALL_PKG_CONFIG_DIR}
  COMPONENT ${XRT_BASE_DEV_COMPONENT}
  )
