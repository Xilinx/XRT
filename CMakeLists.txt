# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.
cmake_minimum_required(VERSION 3.5.0)

if (NOT WIN32)
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message("-- Install prefix is default initialized to ${CMAKE_INSTALL_PREFIX}")
  endif()
else()
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}/xilinx" CACHE PATH "..." FORCE)
    message("-- Install prefix is default initialized to ${CMAKE_INSTALL_PREFIX}")
  endif()
endif()

message("-- CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")


# Enable testing for this directory and below.  This command should be
# in the source directory root because ctest expects to find a test
# file in the build directory root.
enable_testing()

add_subdirectory(src)
