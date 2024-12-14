# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
add_library(core_common_runner_objects OBJECT
  runner.cpp
  cpu.cpp
  )

target_include_directories(core_common_runner_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  )

