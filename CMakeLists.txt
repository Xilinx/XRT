# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
cmake_minimum_required(VERSION 3.5...4.0)

message("-- CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")

# Enable testing for this directory and below.  This command should be
# in the source directory root because ctest expects to find a test
# file in the build directory root.
enable_testing()

add_subdirectory(src)
