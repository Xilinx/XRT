# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
add_library(hip_api_library_objects OBJECT
  hip_device.cpp
  hip_event.cpp
  hip_error.cpp
  hip_memory.cpp
  hip_module.cpp
  hip_stream.cpp
  hip_graph.cpp
)

target_include_directories(hip_api_library_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${XRT_SOURCE_DIR}/runtime_src/core/common/elf
  )

# Vendored HIP headers are third-party; mark SYSTEM so the compiler, clang-tidy
# and Coverity do not analyze/warn on them.
target_include_directories(hip_api_library_objects SYSTEM PRIVATE ${HIP_INCLUDE_DIRS})
