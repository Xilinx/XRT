# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.
add_library(hip_core_library_objects OBJECT
  context.cpp
  device.cpp
  event.cpp
  memory.cpp
  module.cpp
  stream.cpp
  error.cpp
)

target_include_directories(hip_core_library_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${XRT_SOURCE_DIR}/runtime_src/core/include
  ${HIP_INCLUDE_DIRS}
  )
