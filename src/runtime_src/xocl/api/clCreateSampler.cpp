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
#include "xocl/core/object.h"
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"
#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_context           context ,
             cl_bool              normalized_coords ,
             cl_addressing_mode   addressing_mode ,
             cl_filter_mode       filter_mode ,
             cl_int *             errcode_ret )

{
  if( !xocl::config::api_checks())
    return;

}

static cl_sampler
clCreateSampler(cl_context           context ,
                cl_bool              normalized_coords ,
                cl_addressing_mode   addressing_mode ,
                cl_filter_mode       filter_mode ,
                cl_int *             errcode_ret )

{
  validOrError(context,normalized_coords,addressing_mode,filter_mode,errcode_ret);
  xocl::assign(errcode_ret,CL_INVALID_OPERATION);
  return nullptr;
}

}


cl_sampler
clCreateSampler(cl_context           context ,
                cl_bool              normalized_coords ,
                cl_addressing_mode   addressing_mode ,
                cl_filter_mode       filter_mode ,
                cl_int *             errcode_ret )
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clCreateSampler
      (context,normalized_coords,addressing_mode,filter_mode,errcode_ret);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,ex.get_code());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,CL_OUT_OF_HOST_MEMORY);
  }
  return nullptr;
}
