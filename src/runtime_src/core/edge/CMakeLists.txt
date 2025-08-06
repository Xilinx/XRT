# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
#
xrt_add_subdirectory(include)
xrt_add_subdirectory(common)
xrt_add_subdirectory(user)
xrt_add_subdirectory(common_em)
xrt_add_subdirectory(hw_emu)
# We dont need PS Kernel compilation in non-versal devices.
# Using XRT_AIE_BUILD flag as it is enabled for all the versal devices.
if (DEFINED XRT_AIE_BUILD)
        xrt_add_subdirectory(ps_kernels)
endif()
xrt_add_subdirectory(sw_emu)
xrt_add_subdirectory(skd)
