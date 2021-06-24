# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_DIR

install(FILES "${CMAKE_SOURCE_DIR}/../CHANGELOG.rst"
  DESTINATION "${XRT_INSTALL_DIR}/share/doc")

install(FILES "${CMAKE_SOURCE_DIR}/../CONTRIBUTING.rst"
  DESTINATION "${XRT_INSTALL_DIR}/share/doc")

install(FILES "${CMAKE_SOURCE_DIR}/../NOTICE"
  DESTINATION "${XRT_INSTALL_DIR}/share/doc")
