# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.
add_library(hip_core_library_objects OBJECT
  device.cpp
)

target_include_directories(hip_core_library_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  )

