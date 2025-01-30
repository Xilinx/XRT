# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
SET (ICD_FILE_NAME "xilinx.icd")

message("-- Preparing OpenCL ICD ${ICD_FILE_NAME}")

configure_file (
  "${XRT_SOURCE_DIR}/CMake/config/${ICD_FILE_NAME}.in"
  ${ICD_FILE_NAME}
  )

set(OCL_ICD_INSTALL_PREFIX "/etc/OpenCL/vendors")

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${ICD_FILE_NAME}
  DESTINATION ${OCL_ICD_INSTALL_PREFIX}
  COMPONENT ${XRT_BASE_COMPONENT})

