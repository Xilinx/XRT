# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
message( "VITIS is ${MY_VITIS}")
if (DEFINED MY_VITIS)
  if (DEFINED XRT_AIE_BUILD)
    add_subdirectory(sample)
    add_subdirectory(profiling)
  endif()

  add_subdirectory(xrt)
endif()
