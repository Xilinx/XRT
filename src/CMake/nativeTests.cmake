# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
# This cmake file is for native build. Host and target processor are the same.
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_BIN_DIR

message("----CMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}")
message("----CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")
message("----CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}")
message("----PROJECT_BINARY_DIR=${PROJECT_BINARY_DIR}")
message("----XRT_BINARY_DIR=${XRT_BINARY_DIR}")
message("----XRT_INSTALL_DIR=${XRT_INSTALL_DIR}")
message("----XRT_BUILD_INSTALL_DIR=${XRT_BUILD_INSTALL_DIR}")
#enable_testing()

# Run the staged binaries from XRT_BUILD_INSTALL_DIR rather than the build-tree
# binaries. The staged binaries have $ORIGIN-relative RUNPATH entries that
# resolve libxrt_coreutil.so and libxrt_core.so from the same staging lib dir,
# regardless of whether the build-tree binary was linked with RPATH (CentOS/old ld)
# or RUNPATH (Ubuntu/new ld).
#
# XILINX_XRT is set so that on Windows xilinx_xrt() uses the staging area
# instead of the driver store, allowing developers to test local builds.
# On Linux XILINX_XRT is ignored by module_loader (dladdr() is used instead).

add_test(NAME xrt-smi
  COMMAND ${XRT_BUILD_INSTALL_DIR}/${XRT_INSTALL_UNWRAPPED_DIR}/xrt-smi examine
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

if (NOT XRT_UPSTREAM)
  set_tests_properties(xrt-smi PROPERTIES ENVIRONMENT
    "XILINX_XRT=${XRT_BUILD_INSTALL_DIR}")
endif()

if (XRT_XRT OR XRT_ALVEO)
  add_test(NAME xbmgmt2
    COMMAND ${XRT_BUILD_INSTALL_DIR}/${XRT_INSTALL_UNWRAPPED_DIR}/xbmgmt examine -r host
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  if (NOT XRT_UPSTREAM)
    set_tests_properties(xbmgmt2 PROPERTIES ENVIRONMENT
      "XILINX_XRT=${XRT_BUILD_INSTALL_DIR}")
  endif()
endif()
