// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.


#define XCL_DRIVER_DLL_EXPORT
#define XRT_API_SOURCE
#include "xrt/detail/config.h"
#include <chrono>
#include <typeinfo>
#include <xrt.h>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_aie.h>
#include <xrt/xrt_device.h>
#include <xrt/xrt_hw_context.h>
#include <xrt/xrt_kernel.h>
#include <xrt/xrt_uuid.h>
#include <xrt/experimental/xrt_ip.h>
#include <xrt/experimental/xrt_mailbox.h>
#include <xrt/experimental/xrt_module.h>
#include <xrt/experimental/xrt_kernel.h>
#include <xrt/experimental/xrt_profile.h>
#include <xrt/experimental/xrt_queue.h>
#include <xrt/experimental/xrt_error.h>
#include <xrt/experimental/xrt_ext.h>
#include <xrt/experimental/xrt_ini.h>
#include <xrt/experimental/xrt_message.h>
#include <xrt/experimental/xrt_system.h>
#include <xrt/experimental/xrt_aie.h>
#include <xrt/experimental/xrt_version.h>
#include <core/common/api/fence_int.h>
#include <google/protobuf/timestamp.pb.h>
#include <wrapper/tracer.h>
