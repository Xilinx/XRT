# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

message("-- Looking for HIP include files...")
if (NOT WIN32)
  # We should find HIP cmake either in standard cmake locations or in the /opt/rocm location
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/usr/lib/x86_64-linux-gnu/cmake/hip;/usr/lib/x86_64-linux-gnu/cmake/amd_comgr;/opt/rocm/lib/cmake/hip;/opt/rocm/lib/cmake/amd_comgr")
  # hip-config itself requires these other directories to find its dependencies
  set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};/usr/lib/x86_64-linux-gnu/cmake/hip;/usr/lib/x86_64-linux-gnu/cmake/amd_comgr;/usr/lib/x86_64-linux-gnu/cmake/hsa-runtime64;/opt/rocm/lib/cmake/hip;/opt/rocm/lib/cmake/amd_comgr;/opt/rocm/lib/cmake/hsa-runtime64")
else ()
  set(HIP_PLATFORM "amd")
  # HIP SDK installs hip files to C:/Program Files/AMD/ROCm in windows
  # Latest version installed (6.1, 5.7 or whatever available) will be picked
  # Users can set HIP_DIR to location of HIP installation or default path is used
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} $ENV{HIP_DIR} "C:/Program Files/AMD/ROCm/6.2/lib/cmake/hip" "C:/Program Files/AMD/ROCm/6.1/lib/cmake/hip" "C:/Program Files/AMD/ROCm/5.7/lib/cmake/hip")
endif ()

include(hip-config)
message("-- Found at ${HIP_INCLUDE_DIR}")
