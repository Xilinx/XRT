# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
set(XRT_WINDOWS_HEADER_SRC
  types.h
  uuid.h)

install (FILES ${XRT_WINDOWS_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/windows
  COMPONENT ${XRT_DEV_COMPONENT})

install (FILES ${XRT_WINDOWS_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt/detail/windows
  COMPONENT ${XRT_BASE_DEV_COMPONENT})

message("-- XRT OS native header files")
foreach (header ${XRT_WINDOWS_HEADER_SRC})
  message("-- ${header}")
endforeach()
