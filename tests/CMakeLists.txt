# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.
#

CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)
PROJECT(TESTS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_subdirectory(unit_test)
add_subdirectory(xrt)
