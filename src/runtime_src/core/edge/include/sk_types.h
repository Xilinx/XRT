/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc
 * Author(s): Min Ma	<min.ma@xilinx.com>
 *          : Larry Liu	<yliu@xilinx.com>
 *          : Jeff Lin	<jeffli@xilinx.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef __SK_TYPES_H_
#define __SK_TYPES_H_

#include "xclhal2_mpsoc.h"
#include <memory>

/*
 * Helper functions for kernel to use.
 * getHostBO : create a BO handle from given physical address.
 * mapBO     : map BO handle to process's memory space.
 * freeBO    : free BO handle.
 * logMsg    : send log messages to XRT driver for saving as per ini settings
 */
struct XRT_DEPRECATED
sk_operations {
  unsigned int (* getHostBO)(unsigned long paddr, size_t size);
  void *(* mapBO)(unsigned int boHandle, bool write);
  void (* freeBO)(unsigned int boHandle);
  int (* getBufferFd)(unsigned int boHandle);
  int (* logMsg)(xrtLogMsgLevel level, const char* tag,
		       const char* format, ...);
};

/*
 * Each soft kernel fucntion has two arguments.
 * args: provide reg file (data input, output, size etc.,
 *       for soft kernel to run.
 * ops:  provide help functions for soft kernel to use.
 */
XRT_DEPRECATED
typedef int (* kernel_t)(void *args, struct sk_operations *ops);

#pragma message ("sk_types.h is deprecated and will be removed from the distribution directory in a future release.  Please use pscontext.h instead.")

/*
 * Including pscontext.h for backward compatibility with 
 * PS kernels that were using sk_types.h
 */
#include "pscontext.h"

#endif
