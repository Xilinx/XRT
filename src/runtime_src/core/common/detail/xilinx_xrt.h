// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_common_detail_xilinx_xrt_h
#define core_common_detail_xilinx_xrt_h

#ifdef _WIN32
# include "core/common/detail/windows/xilinx_xrt.h"
#else
# include "core/common/detail/linux/xilinx_xrt.h"
#endif

#endif
