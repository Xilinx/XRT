# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#

add_subdirectory(xbutil2)
if (${XRT_NATIVE_BUILD} STREQUAL "yes")
  add_subdirectory(xbmgmt2)
endif()

install (PROGRAMS "./common/xball" DESTINATION ${XRT_INSTALL_BIN_DIR})
