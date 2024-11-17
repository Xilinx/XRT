# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
if(NOT WIN32)
  add_subdirectory(cloud-daemon)
  if (NOT DEFINED NPU)
    add_subdirectory(xbflash.qspi)
  endif(NOT DEFINED NPU)
endif(NOT WIN32)
