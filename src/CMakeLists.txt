# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2018-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
cmake_minimum_required(VERSION 3.5.0)
project(XRT)
set(PROJECT_DESCRIPTION "https://github.com/Xilinx/XRT")

# Include utility functions
include(CMake/utilities.cmake)

# Include supporting CMake functions
include(CMake/unitTestSupport.cmake)

# Exported interface into XRT for include from implemenation
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime_src
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime_src/core/include
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime_src/core/common/gsl/include
  ${XRT_BINARY_DIR}/gen
  ${XRT_BINARY_DIR}
  )

include(CMake/settings.cmake)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${XRT_SOURCE_DIR}/CMake/")

# This makes aiebu submodule use ELFIO from XRT
set(AIEBU_ELFIO_SRC_DIR "${XRT_SOURCE_DIR}/runtime_src/core/common/elf")

if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  # Enable testing for this directory and below.  This command should be
  # in the source directory root because ctest expects to find a test
  # file in the build directory root.
  enable_testing()

  # Temporary native wrapper while Linux code is being ported to windows.
  # When completed the two build flows will once again be merged into one
  # common file
  if (WIN32)
    include(CMake/nativeWin.cmake)
  else()
    include(CMake/nativeLnx.cmake)
  endif()
  xrt_include(CMake/nativeTests.cmake)
else()
  include(CMake/embedded_system.cmake)
endif()
