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

#ifdef _WIN32
# define NOMINMAX
# ifndef CL_TARGET_OPENCL_VERSION
#  define CL_TARGET_OPENCL_VERSION 200
# endif
# include "windows/icd_dispatch.h"
using _cl_icd_dispatch = KHRicdVendorDispatchRec;
#else
# include <ocl_icd.h>
#endif
extern const _cl_icd_dispatch cl_icd_dispatch;

#endif
