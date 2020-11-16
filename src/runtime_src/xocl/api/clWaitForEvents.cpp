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

// Copyright 2016 Xilinx, Inc. All rights reserved.

#include "xocl/core/object.h"
#include "xocl/core/range.h"
#include "detail/event.h"

#include <iostream>

#include "xocl/config.h"
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"

namespace xocl {

static void
validOrError(cl_uint num_events, const cl_event* event_list)
{
  if(!config::api_checks())
    return;

  detail::event::validOrError(num_events, event_list);
}

static cl_int
clWaitForEvents(cl_uint num_events, const cl_event* event_list)
{
  validOrError(num_events, event_list);
  for (auto event : get_range(event_list,event_list+num_events))
    xocl(event)->wait();  
  return CL_SUCCESS;
}

}; // xocl

cl_int 
clWaitForEvents(cl_uint num_events, const cl_event* event_list) 
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clWaitForEvents(num_events,event_list);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}




