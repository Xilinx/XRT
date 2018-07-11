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
#include "xocl/core/memory.h"

#include "detail/memory.h"
#include "xoclProfile.h"

namespace xocl {

static void
validOrError(cl_mem  mem, 
             void    (CL_CALLBACK * pfn_notify)(cl_mem memobj,void* user_data))
{
  if(!config::api_checks())
    return;

  detail::memory::validOrError(mem);
  if (pfn_notify)
    throw error(CL_INVALID_VALUE,"callback function is null");
}

static cl_int
clSetMemObjectDestructorCallback(cl_mem  memobj , 
                                 void (CL_CALLBACK * pfn_notify)(cl_mem memobj,void* user_data), 
                                 void * user_data)
{
  validOrError(memobj,pfn_notify);
  xocl(memobj)->add_dtor_notify([=]{ pfn_notify(memobj,user_data); });
  return CL_SUCCESS;
}

} // xocl

cl_int
clSetMemObjectDestructorCallback(cl_mem memobj , 
                                 void   (CL_CALLBACK * pfn_notify)(cl_mem memobj,void* user_data), 
                                 void*  user_data)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::
      clSetMemObjectDestructorCallback
      (memobj,pfn_notify,user_data);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}


