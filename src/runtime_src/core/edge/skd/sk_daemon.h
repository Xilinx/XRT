/**
 * Copyright (C) 2019-2022 Xilinx, Inc
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

#ifndef __SK_RUNNER_H_
#define __SK_RUNNER_H_

// TO-DO: Remove after XRT Pipeline for edge build is updated to Centos8
#ifndef __x86_64__
#include <boost/stacktrace.hpp>
#endif
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "xclhal2_mpsoc.h"
#include "xrt_skd.h"

void configSoftKernel(const xrtDeviceHandle handle, xclSKCmd* cmd, const int parent_mem_bo, const uint64_t mem_start_paddr, const uint64_t mem_size);

#endif
