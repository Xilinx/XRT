# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc. All rights reserved.
#

set(XBTOP_DIR "${XRT_INSTALL_PYTHON_DIR}/xbtop")
file(MAKE_DIRECTORY ${XBTOP_DIR})

install(FILES ReportDynamicRegions.py DESTINATION ${XBTOP_DIR})
install(FILES ReportMemory.py         DESTINATION ${XBTOP_DIR})
install(FILES ReportPower.py          DESTINATION ${XBTOP_DIR})
install(FILES XBUtil.py               DESTINATION ${XBTOP_DIR})
install(FILES xbtop.py                DESTINATION ${XBTOP_DIR})
