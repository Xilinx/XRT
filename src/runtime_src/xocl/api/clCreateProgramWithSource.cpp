/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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
#include "xocl/core/context.h"
#include "xocl/core/error.h"

#include "plugin/xdp/profile_v2.h"

cl_program
clCreateProgramWithSource(cl_context        context,
                          cl_uint           count,
                          const char **     strings,
                          const size_t *    lengths,
                          cl_int *          errcode_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    throw xocl::error(CL_INVALID_OPERATION,"clCreateProgramWithSource() is not supported, please use clCreateProgramWithBinary().");
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    if (errcode_ret)
      *errcode_ret = ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    if (errcode_ret)
      *errcode_ret = CL_OUT_OF_HOST_MEMORY;
  }
  return nullptr;
}
