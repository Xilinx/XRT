# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.

string(SUBSTRING "${LINUX_VERSION}" 0 1 major_version)
set (PYTHON_3 TRUE)

if (${PYTHON_3})
  install(DIRECTORY
    _xbtop
    DESTINATION ${XRT_INSTALL_PYTHON_DIR} COMPONENT ${XRT_BASE_COMPONENT}
    PATTERN "__pycache__" EXCLUDE)

  install(FILES
    xbtop.py
    DESTINATION ${XRT_INSTALL_PYTHON_DIR} COMPONENT ${XRT_BASE_COMPONENT})

  install (PROGRAMS xbtop
    DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT})
endif()
