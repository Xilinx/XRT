# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
if (XRT_ALVEO)
  add_subdirectory(common_em)
  add_subdirectory(sw_emu)
  add_subdirectory(hw_emu)
endif()
