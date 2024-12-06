# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved
#
set(XRT_DEPRECATED_HEADER_SRC
  xrt.h)

install (FILES ${XRT_DEPRECATED_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/deprecated
  COMPONENT ${XRT_DEV_COMPONENT})

install (FILES ${XRT_DEPRECATED_HEADER_SRC}
  DESTINATION ${XRT_INSTALL_INCLUDE_DIR}/xrt/deprecated
  COMPONENT ${XRT_BASE_DEV_COMPONENT})

message("-- XRT deprecated header files")
foreach (header ${XRT_DEPRECATED_HEADER_SRC})
  message("-- ${header}")
endforeach()
