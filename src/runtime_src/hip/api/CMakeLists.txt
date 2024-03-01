# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.
add_library(hip_api_library_objects OBJECT
  hip_context.cpp
  hip_device.cpp
  hip_event.cpp
  hip_memory.cpp
  hip_module.cpp
  hip_stream.cpp
)

target_include_directories(hip_api_library_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${HIP_INCLUDE_DIRS}
  )
