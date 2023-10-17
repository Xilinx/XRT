# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#
add_subdirectory(include)
add_subdirectory(common)
add_subdirectory(user)
add_subdirectory(common_em)
add_subdirectory(hw_emu)
# We dont need PS Kernel compilation in non-versal devices.
# Using XRT_AIE_BUILD flag as it is enabled for all the versal devices.
if (DEFINED XRT_AIE_BUILD)
        add_subdirectory(ps_kernels)
endif()
add_subdirectory(sw_emu)
add_subdirectory(skd)
