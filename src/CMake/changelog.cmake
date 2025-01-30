# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
# Custom variables imported by this CMake stub which should be defined by parent CMake:
# XRT_INSTALL_DIR

install(FILES
  ${XRT_SOURCE_DIR}/../CHANGELOG.rst
  ${XRT_SOURCE_DIR}/../CONTRIBUTING.rst
  ${XRT_SOURCE_DIR}/../NOTICE
  DESTINATION ${XRT_INSTALL_DIR}/share/doc COMPONENT ${XRT_BASE_COMPONENT})
