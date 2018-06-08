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

#ifndef xocl_api_detail_event_h_
#define xocl_api_detail_event_h_

#include "CL/cl.h"

namespace xocl { namespace detail {

namespace event {

void
validOrError(const cl_event ev);

void 
validOrError(cl_context ctx, cl_uint num_events, const cl_event* event_list, bool check_status=false);

void
validOrError(cl_command_queue cq, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, bool check_status=false);

void
validOrError(cl_uint num_events_in_wait_list, const cl_event* event_wait_list,bool check_status=false);

}

}} // detail,xocl

#endif


