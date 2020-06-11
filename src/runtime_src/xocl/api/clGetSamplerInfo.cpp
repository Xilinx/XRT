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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#include "xocl/config.h"
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/sampler.h"
#include "xocl/core/context.h"
#include "detail/sampler.h"
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"
#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_sampler          sampler ,
             cl_sampler_info     param_name ,
             size_t              param_value_size ,
             void *              param_value ,
             size_t *            param_value_size_ret )
{

  // CL_INVALID_VALUE if param_name is not valid, or if size in bytes
  // specified by param_value_size is < size of return type as
  // described in the table above and param_value is not NULL

  // CL_INVALID_SAMPLER if sampler is a not a valid sampler object.
  detail::sampler::validOrError(sampler);

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
clGetSamplerInfo(cl_sampler          sampler ,
                 cl_sampler_info     param_name ,
                 size_t              param_value_size ,
                 void *              param_value ,
                 size_t *            param_value_size_ret )
{
  validOrError(sampler,param_name,param_value_size,param_value,param_value_size_ret);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };

  switch(param_name){
  case CL_SAMPLER_REFERENCE_COUNT:
    buffer.as<cl_uint>() = xocl::xocl(sampler)->count();
    break;
  case CL_SAMPLER_CONTEXT:
    buffer.as<cl_context>() = xocl::xocl(sampler)->get_context();
    break;
  case CL_SAMPLER_NORMALIZED_COORDS:
    buffer.as<cl_bool>() = xocl::xocl(sampler)->get_norm_mode();
    break;
  case CL_SAMPLER_ADDRESSING_MODE:
    buffer.as<cl_addressing_mode>() = xocl::xocl(sampler)->get_addr_mode();
    break;
  case CL_SAMPLER_FILTER_MODE:
    buffer.as<cl_filter_mode>() = xocl::xocl(sampler)->get_filter_mode();
    break;
  default:
    throw xocl::error(CL_INVALID_VALUE,"clGetSamplerInfo invalid param_name");
    break;
  }

  // Oh well, give up anyway
  throw error(CL_XILINX_UNIMPLEMENTED,"clGetSamplerInfo not implemented");
}

} // xocl

cl_int
clGetSamplerInfo(cl_sampler          sampler ,
                 cl_sampler_info     param_name ,
                 size_t              param_value_size ,
                 void *              param_value ,
                 size_t *            param_value_size_ret )
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clGetSamplerInfo
      (sampler,param_name,param_value_size,param_value,param_value_size_ret);
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}
