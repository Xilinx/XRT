# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
if (XRT_UPSTREAM)
  set(ICD_FILE_NAME "amdxrt.icd")
else()
  set(ICD_FILE_NAME "xilinx.icd")
endif()

message("-- Preparing OpenCL ICD ${ICD_FILE_NAME}")

configure_file (
  "${XRT_SOURCE_DIR}/CMake/config/xilinx.icd.in"
  ${ICD_FILE_NAME}
  )

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${ICD_FILE_NAME}
  DESTINATION ${XRT_INSTALL_ETC_DIR}/OpenCL/vendors
  COMPONENT ${XRT_BASE_COMPONENT})

