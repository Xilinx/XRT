/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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
#ifndef xocl_api_icd_ocl_icd_bindings_h_
#define xocl_api_icd_ocl_icd_bindings_h_

#ifndef CL_TARGET_OPENCL_VERSION
# define CL_TARGET_OPENCL_VERSION 200
#endif

#ifdef _WIN32
# define NOMINMAX
# include "windows/icd_dispatch.h"
using cl_icd_dispatch = KHRicdVendorDispatchRec;
#else
#if (defined (__aarch64__) || defined (__arm__)) && defined (OPENCL_ICD_LOADER)
// In Yocto ocl icd dispatcher is deprecated and
// opencl icd dispatcher is the recommended one.
// Using opencl icd dispatcher for embedded flows.
# include <CL/cl_icd.h>
#else
// All x86 linux distros doesn't have opencl icd dispatcher
// support so using ocl icd dispatcher for x86 flows.
# include <ocl_icd.h>
using cl_icd_dispatch = _cl_icd_dispatch;
#endif
#endif
extern const cl_icd_dispatch cl_icd_dispatch_obj;

#endif
