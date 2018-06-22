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

#include <CL/opencl.h>
#include "xocl/config.h"
#include "xocl/core/error.h"

#include "profile.h"

namespace xocl {

static void
validOrError(cl_context           context ,
             void (CL_CALLBACK *  pfn_notify )(cl_context  program , 
                                               cl_uint printf_data_len , 
                                               char *  printf_data_ptr , 
                                               void *  user_data ),
             void *               user_data )
{
  if (!config::api_checks())
    return;
}

static cl_int
clSetPrintfCallback(cl_context           context ,
                    void (CL_CALLBACK *  pfn_notify )(cl_context  program , 
                                                      cl_uint printf_data_len , 
                                                      char *  printf_data_ptr , 
                                                      void *  user_data ),
                    void *               user_data )
{
  validOrError(context,pfn_notify,user_data);
  throw error(CL_XILINX_UNIMPLEMENTED);
}

} // xocl

cl_int
clSetPrintfCallback(cl_context           context ,
                    void (CL_CALLBACK *  pfn_notify )(cl_context  program , 
                                                      cl_uint printf_data_len , 
                                                      char *  printf_data_ptr , 
                                                      void *  user_data ),
                    void *               user_data )
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clSetPrintfCallback(context,pfn_notify,user_data);
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


