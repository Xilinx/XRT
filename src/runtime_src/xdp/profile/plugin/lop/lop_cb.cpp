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

// The common interface shared between full OpenCL and low overhead OpenCL
#include "xocl/api/plugin/xdp/profile.h"

// To see if low overhead profiling is turned on
#include "core/common/config_reader.h"

#include "xdp/profile/plugin/lop/lop_cb.h"
#include "xdp/profile/plugin/lop/lop_plugin.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/opencl_api_calls.h"
#include "xdp/profile/database/events/opencl_host_events.h"
#include "xrt/util/time.h"

namespace xdp {

  static LowOverheadProfilingPlugin lopPluginInstance ;

  static void lop_cb_log_function_start(const char* functionName,
					long long queueAddress,
					unsigned int functionID)
  {
    // Since these are OpenCL level events, we must use the OpenCL
    //  level time functions to get the proper value of time zero.
    double timestamp = xrt::time_ns() ;
    VPDatabase* db = lopPluginInstance.getDatabase() ;

    if (queueAddress != 0) 
      (db->getStaticInfo()).addCommandQueueAddress(queueAddress) ;

    VTFEvent* event = new OpenCLAPICall(0,
					timestamp,
					functionID,
					(db->getDynamicInfo()).addString(functionName),
					queueAddress
					) ;
    (db->getDynamicInfo()).addEvent(event) ;
    (db->getDynamicInfo()).markStart(functionID, event->getEventId()) ;
  }

  static void lop_cb_log_function_end(const char* functionName,
				      long long queueAddress,
				      unsigned int functionID)
  {
    double timestamp = xrt::time_ns() ;
    VPDatabase* db = lopPluginInstance.getDatabase() ;

    uint64_t start = (db->getDynamicInfo()).matchingStart(functionID) ;

    VTFEvent* event = new OpenCLAPICall(start,
					timestamp,
					functionID,
					(db->getDynamicInfo()).addString(functionName),
					queueAddress) ;
    (db->getDynamicInfo()).addEvent(event) ;
  }

  void register_low_overhead_profile_callbacks()
  {
    // Set up the callbacks for logging the start and end of function calls.
    //  These use the same hooks as the normal profiling.
    xocl::profile::register_cb_log_function_start(lop_cb_log_function_start);
    xocl::profile::register_cb_log_function_end(lop_cb_log_function_end);    
  }

} // end namespace xdp

// This function is called from XRT once when the library is initially loaded
extern "C" 
void initLOP()
{
  xdp::register_low_overhead_profile_callbacks() ;
}
