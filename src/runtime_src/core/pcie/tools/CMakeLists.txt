# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
add_subdirectory(cloud-daemon)

if(XRT_ALVEO)
  add_subdirectory(xbflash.qspi)
endif(XRT_ALVEO)
