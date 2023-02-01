# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
add_library(core_pciecommon_objects OBJECT
  aie_stubs.cpp
  asd_parser.cpp
  device_pcie.cpp
  system_pcie.cpp
  )

target_include_directories(core_pciecommon_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${XRT_BINARY_DIR}/gen
  )
