/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2022, Advanced Micro Devices, Inc. All rights reserved.
 */

/*
 * Test C compilation with xrt header files
 */
#include "core/include/xrt/xrt_aie.h"
#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_device.h"
#include "core/include/xrt/xrt_kernel.h"
#include "core/include/xrt/xrt_uuid.h"
#include "core/include/xrt/xrt_graph.h"
#include "core/include/xrt/experimental/xrt_error.h"
#include "core/include/xrt/experimental/xrt_ini.h"
#include "core/include/xrt/experimental/xrt_xclbin.h"
