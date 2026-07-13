# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 - 2025 Advanced Micro Devices, Inc. All rights reserved.

# The xrt_hip library always builds against the minimal set of HIP host headers
# vendored under detail/ (produced by detail/get_hip_headers.py). No external
# ROCm/HIP install is required.
set(HIP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/detail")
set(HIP_INCLUDE_DIRS "${HIP_INCLUDE_DIR}")
message("-- Using vendored HIP headers at ${HIP_INCLUDE_DIR}")
