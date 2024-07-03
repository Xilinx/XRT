# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
add_library(hip_api_library_objects OBJECT
  hipDeviceGet.cpp
)

target_include_directories(hip_api_library_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${HIP_INCLUDE_DIRS}
  )
