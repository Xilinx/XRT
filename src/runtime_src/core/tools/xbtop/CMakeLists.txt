# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.
#

message(${LINUX_FLAVOR} ${LINUX_VERSION})
string(SUBSTRING "${LINUX_VERSION}" 0 1 major_version)
set (PYTHON_3 TRUE)

if (${LINUX_FLAVOR} STREQUAL "Amazon")
  set (PYTHON_3 FALSE)
elseif ((${LINUX_FLAVOR} STREQUAL "CentOS") AND (${major_version} STREQUAL "7"))
  set (PYTHON_3 FALSE)
endif ()

if (${PYTHON_3})
    install(FILES ReportDynamicRegions.py DESTINATION ${XRT_INSTALL_PYTHON_DIR})
    install(FILES ReportMemory.py         DESTINATION ${XRT_INSTALL_PYTHON_DIR})
    install(FILES ReportPower.py          DESTINATION ${XRT_INSTALL_PYTHON_DIR})
    install(FILES XBUtil.py               DESTINATION ${XRT_INSTALL_PYTHON_DIR})
    install(FILES xbtop.py               DESTINATION ${XRT_INSTALL_PYTHON_DIR})

    install (PROGRAMS xbtop DESTINATION ${XRT_INSTALL_BIN_DIR})
endif()
