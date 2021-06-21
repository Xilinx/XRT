# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
if(NOT WIN32)
  add_subdirectory(xbutil)
  add_subdirectory(xbmgmt)
  add_subdirectory(cloud-daemon)
  add_subdirectory(xbflash.qspi)
endif(NOT WIN32)

