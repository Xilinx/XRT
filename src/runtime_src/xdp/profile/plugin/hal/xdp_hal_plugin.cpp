/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include <iostream>

#define XDP_PLUGIN_SOURCE

#include "xdp_hal_plugin_interface.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/hal_api_calls.h"
#include "xdp/profile/database/events/opencl_host_events.h"
#include "core/common/time.h"

#include "hal_plugin.h"

namespace xdp {

  // This object is created when the plugin library is loaded
  static HALPlugin halPluginInstance ;

  static void generic_log_function_start(const char* functionName, uint64_t id)
  {
    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VPDatabase* db = halPluginInstance.getDatabase() ;

    // Update counters
    (db->getStats()).logFunctionCallStart(functionName, timestamp) ;

    // Update trace
    VTFEvent* event =
      new HALAPICall(0,
                     timestamp,
                     (db->getDynamicInfo()).addString(functionName));
    (db->getDynamicInfo()).addEvent(event) ;
    (db->getDynamicInfo()).markStart(id, event->getEventId()) ;
  }

  static void generic_log_function_end(const char* functionName, uint64_t id)
  {
    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VPDatabase* db = halPluginInstance.getDatabase() ;
  
    // Update counters
    (db->getStats()).logFunctionCallEnd(functionName, timestamp) ;

    // Update trace
    VTFEvent* event =
      new HALAPICall((db->getDynamicInfo()).matchingStart(id),
		     timestamp,
		     (db->getDynamicInfo()).addString(functionName));
    (db->getDynamicInfo()).addEvent(event) ;
  }

  static void write_bo_start(const char* name, uint64_t id,
                             uint64_t bufferId, uint64_t size)
  {
    generic_log_function_start(name, id) ;

    // Also create a buffer transfer event
    VPDatabase* db = halPluginInstance.getDatabase() ;

    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VTFEvent* event = new BufferTransfer(0, timestamp, WRITE_BUFFER, size);
    (db->getDynamicInfo()).addEvent(event);
    (db->getDynamicInfo()).markStart(bufferId, event->getEventId());
  }

  static void write_bo_end(const char* name, uint64_t id, uint64_t bufferId)
  {
    generic_log_function_end(name, id) ;

    // Add trace event for end of Buffer Transfer
    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VPDatabase* db = halPluginInstance.getDatabase();
    VTFEvent* event =
      new BufferTransfer(db->getDynamicInfo().matchingStart(bufferId),
                         timestamp, WRITE_BUFFER);
    (db->getDynamicInfo()).addEvent(event);
  }

  static void read_bo_start(const char* name, uint64_t id,
                            uint64_t bufferId, uint64_t size)
  {
    generic_log_function_start(name, id) ;

    // Also create a buffer transfer event
    VPDatabase* db = halPluginInstance.getDatabase() ;

    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VTFEvent* event = new BufferTransfer(0, timestamp, READ_BUFFER, size);
    (db->getDynamicInfo()).addEvent(event);
    (db->getDynamicInfo()).markStart(bufferId, event->getEventId());
  }

  static void read_bo_end(const char* name, uint64_t id, uint64_t bufferId)
  {
    generic_log_function_end(name, id) ;

    // Add trace event for end of Buffer Transfer
    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VPDatabase* db = halPluginInstance.getDatabase();
    VTFEvent* event =
      new BufferTransfer(db->getDynamicInfo().matchingStart(bufferId),
                         timestamp, READ_BUFFER);
    (db->getDynamicInfo()).addEvent(event);
  }

} //  xdp

extern "C"
void hal_generic_cb(bool isStart, const char* name, unsigned long long int id)
{
  if(!xdp::VPDatabase::alive() || !xdp::HALPlugin::alive())
    return;

  if (isStart)
    xdp::generic_log_function_start(name, static_cast<uint64_t>(id)) ;
  else
    xdp::generic_log_function_end(name, static_cast<uint64_t>(id)) ;
}

extern "C"
void buffer_transfer_cb(bool isWrite, bool isStart, const char* name,
                        unsigned long long int id,
                        unsigned long long int bufferId,
                        unsigned long long int size)
{
  if(!xdp::VPDatabase::alive() || !xdp::HALPlugin::alive())
    return;

  if (isWrite) {
    if (isStart) {
      xdp::write_bo_start(name,
                          static_cast<uint64_t>(id),
                          static_cast<uint64_t>(bufferId),
                          static_cast<uint64_t>(size)) ;
    }
    else {
      xdp::write_bo_end(name,
                        static_cast<uint64_t>(id),
                        static_cast<uint64_t>(bufferId)) ;
    }
  }
  else { // isRead
    if (isStart) {
      xdp::read_bo_start(name,
                         static_cast<uint64_t>(id),
                         static_cast<uint64_t>(bufferId),
                         static_cast<uint64_t>(size)) ;
    }
    else {
      xdp::read_bo_end(name,
                       static_cast<uint64_t>(id),
                       static_cast<uint64_t>(bufferId)) ;
    }
  }
}

