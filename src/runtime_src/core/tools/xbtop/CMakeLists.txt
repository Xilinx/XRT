# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.
#

install(FILES ReportDynamicRegions.py DESTINATION ${XRT_INSTALL_PYTHON_DIR})
install(FILES ReportMemory.py         DESTINATION ${XRT_INSTALL_PYTHON_DIR})
install(FILES ReportPower.py          DESTINATION ${XRT_INSTALL_PYTHON_DIR})
install(FILES XBUtil.py               DESTINATION ${XRT_INSTALL_PYTHON_DIR})

install (PROGRAMS xbtop DESTINATION ${XRT_INSTALL_BIN_DIR})
