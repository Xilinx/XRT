# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
message("-- Preparing XRT pkg-config")

if (${LINUX_FLAVOR} MATCHES "^(ubuntu)")
  set(XRT_PKG_CONFIG_DIR "/usr/lib/pkgconfig")
elseif (${LINUX_FLAVOR} MATCHES "^(rhel|centos|amzn|fedora|sles|almalinux)")
  set(XRT_PKG_CONFIG_DIR "/usr/lib64/pkgconfig")
else ()
  set(XRT_PKG_CONFIG_DIR "/usr/share/pkgconfig")
endif ()

configure_file (
  ${XRT_SOURCE_DIR}/CMake/config/xrt.pc.in
  xrt.pc
  @ONLY
  )
install (
  FILES ${CMAKE_CURRENT_BINARY_DIR}/xrt.pc
  DESTINATION ${XRT_PKG_CONFIG_DIR}
  COMPONENT ${XRT_BASE_DEV_COMPONENT}
  )
