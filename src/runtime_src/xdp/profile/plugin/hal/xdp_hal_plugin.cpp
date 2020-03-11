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

#include <iostream>

#define XDP_SOURCE

#include "xdp_hal_plugin_interface.h"
#include "xdp_hal_plugin.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/hal_api_calls.h"
#include "xdp/profile/database/events/opencl_host_events.h"
#include "core/common/time.h"

#include "hal_plugin.h"

namespace xdp {

  // This object is created when the plugin library is loaded
  static HALPlugin halPluginInstance ;

  static void log_function_start(void* payload, const char* functionName)
  {
    double timestamp = xrt_core::time_ns() ;

    CBPayload* decoded = reinterpret_cast<CBPayload*>(payload) ;
    VPDatabase* db = halPluginInstance.getDatabase() ;

    // Update counters
    (db->getStats()).logFunctionCallStart(functionName, timestamp) ;

    // Update trace
    VTFEvent* event = new HALAPICall(0,
                          timestamp,
                          (db->getDynamicInfo()).addString(functionName));
    (db->getDynamicInfo()).addEvent(event) ;
    (db->getDynamicInfo()).markStart(decoded->idcode, event->getEventId()) ;
    return;
  }

  static void log_function_end(void* payload, const char* functionName)
  {
    double timestamp = xrt_core::time_ns() ;

    CBPayload* decoded = reinterpret_cast<CBPayload*>(payload) ;
    VPDatabase* db = halPluginInstance.getDatabase() ;
  
    // Update counters
    (db->getStats()).logFunctionCallEnd(functionName, timestamp) ;

    // Update trace
    VTFEvent* event = new HALAPICall((db->getDynamicInfo()).matchingStart(decoded->idcode),
				                  timestamp,
				                  (db->getDynamicInfo()).addString(functionName));
    (db->getDynamicInfo()).addEvent(event) ;
    return;
  }

  static void alloc_bo_start(void* payload) {  
    log_function_start(payload, "AllocBO") ;
  }

  static void alloc_bo_end(void* payload) {
    log_function_end(payload, "AllocBO") ;
  }

  static void alloc_userptr_bo_start(void* payload) {  
    log_function_start(payload, "AllocUserPtrBO") ;
  }

  static void alloc_userptr_bo_end(void* payload) {
    log_function_end(payload, "AllocUserPtrBO") ;
  }

  static void free_bo_start(void* payload) {
    log_function_start(payload, "FreeBO") ;
  }

  static void free_bo_end(void* payload) {
    log_function_end(payload, "FreeBO") ;
  }

  static void write_bo_start(void* payload) {
    log_function_start(payload, "WriteBO") ;

    BOTransferCBPayload* pLoad = reinterpret_cast<BOTransferCBPayload*>(payload);

    // Also log the amount of data transferred
    uint64_t deviceId = halPluginInstance.getDeviceId(pLoad->basePayload.deviceHandle);
    VPDatabase* db = halPluginInstance.getDatabase() ;
    (db->getStats()).logMemoryTransfer(deviceId,
				       DeviceMemoryStatistics::BUFFER_WRITE,
				       pLoad->size) ;

    // Add trace event for start of Buffer Transfer
    double timestamp = xrt_core::time_ns();
    VTFEvent* event = new BufferTransfer(0, timestamp, WRITE_BUFFER, pLoad->size);
    (db->getDynamicInfo()).addEvent(event);
    (db->getDynamicInfo()).markStart(pLoad->bufferTransferId, event->getEventId());
  }

  static void write_bo_end(void* payload) {
    log_function_end(payload, "WriteBO") ;

    BOTransferCBPayload* pLoad = reinterpret_cast<BOTransferCBPayload*>(payload);

    // Add trace event for end of Buffer Transfer
    double timestamp = xrt_core::time_ns();
    VPDatabase* db = halPluginInstance.getDatabase();
    VTFEvent* event = new BufferTransfer(
                          (db->getDynamicInfo()).matchingStart(pLoad->bufferTransferId),
                          timestamp, WRITE_BUFFER);
    (db->getDynamicInfo()).addEvent(event);
  }

  static void read_bo_start(void* payload) {
    log_function_start(payload, "ReadBO") ;

    BOTransferCBPayload* pLoad = reinterpret_cast<BOTransferCBPayload*>(payload);

    // Also log the amount of data transferred
    uint64_t deviceId = halPluginInstance.getDeviceId(pLoad->basePayload.deviceHandle);
    VPDatabase* db = halPluginInstance.getDatabase() ;
    (db->getStats()).logMemoryTransfer(deviceId,
				       DeviceMemoryStatistics::BUFFER_READ,
				       pLoad->size) ;

    // Add trace event for start of Buffer Transfer
    double timestamp = xrt_core::time_ns();
    VTFEvent* event = new BufferTransfer(0, timestamp, READ_BUFFER, pLoad->size);
    (db->getDynamicInfo()).addEvent(event);
    (db->getDynamicInfo()).markStart(pLoad->bufferTransferId, event->getEventId());
  }

  static void read_bo_end(void* payload) {
    log_function_end(payload, "ReadBO") ;

    BOTransferCBPayload* pLoad = reinterpret_cast<BOTransferCBPayload*>(payload);

    // Add trace event for end of Buffer Transfer
    double timestamp = xrt_core::time_ns();
    VPDatabase* db = halPluginInstance.getDatabase();
    VTFEvent* event = new BufferTransfer(
                          (db->getDynamicInfo()).matchingStart(pLoad->bufferTransferId),
                          timestamp, READ_BUFFER);
    (db->getDynamicInfo()).addEvent(event);
  }

  static void map_bo_start(void* payload) {
    log_function_start(payload, "MapBO") ;
  }

  static void map_bo_end(void* payload) {
    log_function_end(payload, "MapBO") ;
  }

  static void sync_bo_start(void* payload) {
    log_function_start(payload, "SyncBO") ;

    SyncBOCBPayload* pLoad = reinterpret_cast<SyncBOCBPayload*>(payload);

    // Also log the amount of data transferred
    uint64_t deviceId = halPluginInstance.getDeviceId(pLoad->basePayload.deviceHandle);
    VPDatabase* db = halPluginInstance.getDatabase() ;
    (db->getStats()).logMemoryTransfer(deviceId,
                       (pLoad->isWriteToDevice ? DeviceMemoryStatistics::BUFFER_WRITE : DeviceMemoryStatistics::BUFFER_READ),
				       pLoad->size) ;

    // Add trace event for start of Buffer Transfer
    double timestamp = xrt_core::time_ns();
    VTFEvent* event = new BufferTransfer(0, timestamp,
                            ((pLoad->isWriteToDevice) ? WRITE_BUFFER : READ_BUFFER), pLoad->size);
    (db->getDynamicInfo()).addEvent(event);
    (db->getDynamicInfo()).markStart(pLoad->bufferTransferId, event->getEventId());
  }

  static void sync_bo_end(void* payload) {
    log_function_end(payload, "SyncBO") ;

    SyncBOCBPayload* pLoad = reinterpret_cast<SyncBOCBPayload*>(payload);

    // Add trace event for end of Buffer Transfer
    double timestamp = xrt_core::time_ns();
    VPDatabase* db = halPluginInstance.getDatabase();
    VTFEvent* event = new BufferTransfer(
                          (db->getDynamicInfo()).matchingStart(pLoad->bufferTransferId),
                          timestamp,
                          ((pLoad->isWriteToDevice) ? WRITE_BUFFER : READ_BUFFER));
    (db->getDynamicInfo()).addEvent(event);
  }

  static void copy_bo_start(void* payload) {
    log_function_start(payload, "CopyBO") ;
  }

  static void copy_bo_end(void* payload) {
    log_function_end(payload, "CopyBO") ;
  }

  static void unmgd_read_start(void* payload) {
    UnmgdPreadPwriteCBPayload* pLoad = 
      reinterpret_cast<UnmgdPreadPwriteCBPayload*>(payload);
    log_function_start(&(pLoad->basePayload), "UnmgdRead") ;
    
    // Also log the amount of data transferred
    uint64_t deviceId = halPluginInstance.getDeviceId(pLoad->basePayload.deviceHandle);
    VPDatabase* db = halPluginInstance.getDatabase() ;
    (db->getStats()).logMemoryTransfer(deviceId,
				       DeviceMemoryStatistics::UNMANAGED_READ, 
				       pLoad->count) ;
  }

  static void unmgd_read_end(void* payload) {
    UnmgdPreadPwriteCBPayload* pLoad = 
      reinterpret_cast<UnmgdPreadPwriteCBPayload*>(payload) ;
    log_function_end(&(pLoad->basePayload), "UnmgdRead") ;
  }

  static void unmgd_write_start(void* payload) {
    UnmgdPreadPwriteCBPayload* pLoad = 
      reinterpret_cast<UnmgdPreadPwriteCBPayload*>(payload);
    log_function_start(&(pLoad->basePayload), "UnmgdWrite") ;

    // Also log the amount of data transferred
    uint64_t deviceId = halPluginInstance.getDeviceId(pLoad->basePayload.deviceHandle);
    VPDatabase* db = halPluginInstance.getDatabase() ;
    (db->getStats()).logMemoryTransfer(deviceId,
				       DeviceMemoryStatistics::UNMANAGED_WRITE,
				       pLoad->count) ;
  }

  static void unmgd_write_end(void* payload) {
    UnmgdPreadPwriteCBPayload* pLoad = 
      reinterpret_cast<UnmgdPreadPwriteCBPayload*>(payload) ;
    log_function_end(&(pLoad->basePayload), "UnmgdWrite") ;
  }

  static void read_start(void* payload) {
    ReadWriteCBPayload* pLoad = reinterpret_cast<ReadWriteCBPayload*>(payload);
    log_function_start(&(pLoad->basePayload), "xclRead") ;

    // Also log the amount of data transferred
    uint64_t deviceId = halPluginInstance.getDeviceId(pLoad->basePayload.deviceHandle);
    VPDatabase* db = halPluginInstance.getDatabase() ;
    (db->getStats()).logMemoryTransfer(deviceId,
				                DeviceMemoryStatistics::XCLREAD, pLoad->size) ;
  }

  static void read_end(void* payload) {  
    log_function_end(payload, "xclRead") ;
  }

  static void write_start(void* payload) {
    ReadWriteCBPayload* pLoad = reinterpret_cast<ReadWriteCBPayload*>(payload);
    log_function_start(&(pLoad->basePayload), "xclWrite") ;

    // Also log the amount of data transferred
    uint64_t deviceId = halPluginInstance.getDeviceId(pLoad->basePayload.deviceHandle);
    VPDatabase* db = halPluginInstance.getDatabase() ;
    (db->getStats()).logMemoryTransfer(deviceId,
				       DeviceMemoryStatistics::XCLWRITE, 
				       pLoad->size) ;
  }

  static void write_end(void* payload) {
    log_function_end(payload, "xclWrite") ;
  }

  static void probe_start(void* payload) {
    log_function_start(payload, "Probe") ;
  }

  static void probe_end(void* payload) {
    log_function_end(payload, "Probe") ;
  }

  static void lock_device_start(void* payload) {
    log_function_start(payload, "LockDevice") ;
  }

  static void lock_device_end(void* payload) {
    log_function_end(payload, "LockDevice") ;
  }

  static void unlock_device_start(void* payload) {
    log_function_start(payload, "UnLockDevice") ;
  }

  static void unlock_device_end(void* payload) {
    log_function_end(payload, "UnLockDevice") ;
  }

  static void open_start(void* payload) {
    log_function_start(payload, "Open") ;
  }

  static void open_end(void* payload) {
    log_function_end(payload, "Open") ;
  }

  static void close_start(void* payload) {
    log_function_start(payload, "Close") ;
  }

  static void close_end(void* payload) {
    log_function_end(payload, "Close") ;
  }

  static void open_context_start(void* payload) {
    log_function_start(payload, "OpenContext") ;
  }

  static void open_context_end(void* payload) {
    log_function_end(payload, "OpenContext") ;
  }

  static void close_context_start(void* payload) {
    log_function_start(payload, "CloseContext") ;
  }

  static void close_context_end(void* payload) {
    log_function_end(payload, "CloseContext") ;
  }

  static void load_xclbin_start(void* payload) {
    // The xclbin is about to be loaded, so flush any device information
    //  into the database
    XclbinCBPayload* pLoad = reinterpret_cast<XclbinCBPayload*>(payload) ;

    // Before we load a new xclbin, make sure we read all of the 
    //  device data into the database.
    halPluginInstance.readDeviceInfo((pLoad->basePayload).deviceHandle) ;
    halPluginInstance.flushDeviceInfo((pLoad->basePayload).deviceHandle) ;
  }

  static void load_xclbin_end(void* payload) {
    // The xclbin has been loaded, so update all the static information
    //  in our database.
    XclbinCBPayload* pLoad = reinterpret_cast<XclbinCBPayload*>(payload) ;
    halPluginInstance.updateDevice((pLoad->basePayload).deviceHandle, pLoad->binary);
  }

  static void unknown_cb_type(void* /*payload*/) {
    return;
  }

} //  xdp

void hal_level_xdp_cb_func(HalCallbackType cb_type, void* payload)
{
  if(!xdp::VPDatabase::alive()) {
    return;
  }

  switch (cb_type) {
    case HalCallbackType::ALLOC_BO_START:
      xdp::alloc_bo_start(payload);
      break;
    case HalCallbackType::ALLOC_BO_END:
      xdp::alloc_bo_end(payload);
      break;
    case HalCallbackType::ALLOC_USERPTR_BO_START:
      xdp::alloc_userptr_bo_start(payload);
      break;
    case HalCallbackType::ALLOC_USERPTR_BO_END:
      xdp::alloc_userptr_bo_end(payload);
      break;
    case HalCallbackType::FREE_BO_START:
      xdp::free_bo_start(payload);
      break;
    case HalCallbackType::FREE_BO_END:
      xdp::free_bo_end(payload);
      break;
    case HalCallbackType::WRITE_BO_START:
      xdp::write_bo_start(payload);
      break;
    case HalCallbackType::WRITE_BO_END:
      xdp::write_bo_end(payload);
      break;
    case HalCallbackType::READ_BO_START:
      xdp::read_bo_start(payload);
      break;
    case HalCallbackType::READ_BO_END:
      xdp::read_bo_end(payload);
      break;
    case HalCallbackType::MAP_BO_START:
      xdp::map_bo_start(payload);
      break;
    case HalCallbackType::MAP_BO_END:
      xdp::map_bo_end(payload);
      break;
    case HalCallbackType::SYNC_BO_START:
      xdp::sync_bo_start(payload);
      break;
    case HalCallbackType::SYNC_BO_END:
      xdp::sync_bo_end(payload);
      break;
    case HalCallbackType::COPY_BO_START:
      xdp::copy_bo_start(payload);
      break;
    case HalCallbackType::COPY_BO_END:
      xdp::copy_bo_end(payload);
      break;
    case HalCallbackType::UNMGD_READ_START:
      xdp::unmgd_read_start(payload);
      break;
    case HalCallbackType::UNMGD_READ_END:
      xdp::unmgd_read_end(payload);
      break;
    case HalCallbackType::UNMGD_WRITE_START:
      xdp::unmgd_write_start(payload);
      break;
    case HalCallbackType::UNMGD_WRITE_END:
      xdp::unmgd_write_end(payload);
      break;
    case HalCallbackType::READ_START:
      xdp::read_start(payload);
      break;
    case HalCallbackType::READ_END:
      xdp::read_end(payload);
      break;
    case HalCallbackType::WRITE_START:
      xdp::write_start(payload);
      break;
    case HalCallbackType::WRITE_END:
      xdp::write_end(payload);
      break;
    case HalCallbackType::PROBE_START:
      xdp::probe_start(payload);
      break;
    case HalCallbackType::PROBE_END:
      xdp::probe_end(payload);
      break;
    case HalCallbackType::LOCK_DEVICE_START:
      xdp::lock_device_start(payload);
      break;
    case HalCallbackType::LOCK_DEVICE_END:
      xdp::lock_device_end(payload);
      break;
    case HalCallbackType::UNLOCK_DEVICE_START:
      xdp::unlock_device_start(payload);
      break;
    case HalCallbackType::UNLOCK_DEVICE_END:
      xdp::unlock_device_end(payload);
      break;
    case HalCallbackType::OPEN_START:
      xdp::open_start(payload);
      break;
    case HalCallbackType::OPEN_END:
      xdp::open_end(payload);
      break;
    case HalCallbackType::CLOSE_START:
      xdp::close_start(payload);
      break;
    case HalCallbackType::CLOSE_END:
      xdp::close_end(payload);
      break;
    case HalCallbackType::OPEN_CONTEXT_START:
      xdp::open_context_start(payload);
      break;
    case HalCallbackType::OPEN_CONTEXT_END:
      xdp::open_context_end(payload);
      break;
    case HalCallbackType::CLOSE_CONTEXT_START:
      xdp::close_context_start(payload);
      break;
    case HalCallbackType::CLOSE_CONTEXT_END:
      xdp::close_context_end(payload);
      break;
    case HalCallbackType::LOAD_XCLBIN_START:
      xdp::load_xclbin_start(payload) ;
      break ;
    case HalCallbackType::LOAD_XCLBIN_END:
      xdp::load_xclbin_end(payload) ;
      break ;
    default: 
      xdp::unknown_cb_type(payload);
      break;
  }
  return;
}
