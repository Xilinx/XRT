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

add_test(NAME xbutil2
  COMMAND ${XRT_BINARY_DIR}/runtime_src/core/tools/xbutil2/xbutil2 examine
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(xbutil2 PROPERTIES ENVIRONMENT
  "XILINX_XRT=${XRT_BUILD_INSTALL_DIR}")

add_test(NAME xbmgmt2
  COMMAND ${XRT_BINARY_DIR}/runtime_src/core/tools/xbmgmt2/xbmgmt2 examine -r host
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(xbmgmt2 PROPERTIES ENVIRONMENT
  "XILINX_XRT=${XRT_BUILD_INSTALL_DIR}")

add_test(NAME python_binding
  COMMAND ${PYTHON_EXECUTABLE} "${XRT_SOURCE_DIR}/../tests/python/200_binding/200_main.py"
  WORKING_DIRECTORY ${XRT_BINARY_DIR})

set_tests_properties(python_binding PROPERTIES ENVIRONMENT
  "PYTHONPATH=${XRT_BUILD_INSTALL_DIR}/python;XILINX_XRT=${XRT_BUILD_INSTALL_DIR}")
