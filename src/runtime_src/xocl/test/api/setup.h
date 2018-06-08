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

#include "CL/cl.h"

struct ocl_sw_emulation
{
  cl_platform_id platform = nullptr;
  cl_device_id device = nullptr;
  cl_context context = nullptr;

  ocl_sw_emulation()
  {
    cl_int err = CL_SUCCESS;
    BOOST_REQUIRE_EQUAL(clGetPlatformIDs(1,&platform,nullptr),CL_SUCCESS);
    BOOST_REQUIRE_EQUAL(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ACCELERATOR, 1, &device, nullptr),CL_SUCCESS);
    BOOST_REQUIRE(context=clCreateContext(0, 1, &device, nullptr, nullptr, &err));
    BOOST_REQUIRE_EQUAL(err,CL_SUCCESS);
  }    

  ~ocl_sw_emulation()
  {
    clReleaseContext(context);
    clReleaseDevice(device);
  }

};


