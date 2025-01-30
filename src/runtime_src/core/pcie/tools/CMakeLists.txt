# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
if(XRT_ALVEO)
  add_subdirectory(cloud-daemon)
  add_subdirectory(xbflash.qspi)
endif(XRT_ALVEO)
