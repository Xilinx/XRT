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

#ifndef xocl_api_detail_device_h_
#define xocl_api_detail_device_h_

#include "xocl/config.h"
#include "CL/cl.h"

namespace xocl { namespace detail {

namespace device {

void
validOrError(const cl_device_id device);

void
validOrError(const cl_device_type device_type);

void
validOrError(const cl_program, cl_device_id device);

void
validOrError(const cl_device_id device, const cl_kernel kernel);

void
validOrError(cl_uint num_devices, const cl_device_id* device_list);

void
validOrError(cl_program program, cl_uint num_devices, const cl_device_id* device_list);

void
validOrError(cl_context context, cl_uint num_devices, const cl_device_id* device_list);

void
validOrError(cl_platform_id platform, cl_uint num_devices, const cl_device_id* device_list);

}

}} // detail,xocl

#endif
