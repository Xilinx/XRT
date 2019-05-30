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

/**
 * This file contains the profiling callback actions registered to XRT
 */

#include "xocl_profile_cb.h"
#include "ocl_profiler.h"
#include "xdp/rt_singleton.h"
#include "xdp/profile/core/rt_profile.h"

namespace xdp {

bool isProfilingOn() {
  auto profiler = OCLProfiler::Instance();
  return (profiler == nullptr) ? false : profiler->applicationProfilingOn();
}

// Create string to uniquely identify event
std::string get_event_string(xocl::event* currEvent) {
#if 0
  std::stringstream sstr;
  sstr << std::showbase << std::hex << reinterpret_cast<uint64_t>(currEvent);
  return sstr.str();
#else
  return currEvent->get_suid();
#endif
}

// Find all events that currEvent is dependent upon, return string
// Note that, this function calls try_get_chain() which locks the event object
// So any functions called while iterating on the chain should not lock the event
std::string get_event_dependencies_string(xocl::event* currEvent) {
  std::stringstream sstr;

  try {
    // consider all events, including user events that are not in any command queue
    xocl::range_lock<xocl::event::event_iterator_type>&& currRange = currEvent->try_get_chain();

    if (currRange.size() == 0) {
      sstr << "None";
    }
    else {
      bool first = true;
      for (auto it = currRange.begin(); it != currRange.end(); ++it) {
#if 0
        sstr << (first ? "":"|") << std::showbase << std::hex
             << reinterpret_cast<uint64_t>(it->get());
#else
        xocl::event* depEvent = *it;
        sstr << (first ? "":"|") << depEvent->get_suid();
#endif
        first = false;
      }
    }
  }
  catch (const xocl::error &err) {
    XOCL_DEBUGF("IGNORE: %s\n", err.what());
    sstr << "None";
  }

  return sstr.str();
}

static xdp::RTUtil::e_profile_command_state
event_status_to_profile_state(cl_int status)
{
  static const std::map<cl_int, xdp::RTUtil::e_profile_command_state> tbl
  {
    {CL_QUEUED,    xdp::RTUtil::QUEUE}
   ,{CL_SUBMITTED, xdp::RTUtil::SUBMIT}
   ,{CL_RUNNING,   xdp::RTUtil::START}
   ,{CL_COMPLETE,  xdp::RTUtil::END}
  };

  auto itr = tbl.find(status);
  if (itr==tbl.end())
    throw std::runtime_error("bad event status '" + std::to_string(status) + "'");
  return (*itr).second;
}

/*
 * Lambda generators called from openCL APIs
 *
 */
void
cb_action_ndrange(xocl::event* event,cl_int status,const std::string& cu_name, cl_kernel kernel,
                  std::string kname, std::string xname, size_t workGroupSize, const size_t* globalWorkDim,
                  const size_t* localWorkDim, unsigned int programId)
{
    if (!isProfilingOn())
      return;

    // Create string to specify event and its dependencies
    std::string eventStr;
    std::string dependStr;
    if (status == CL_RUNNING || status == CL_COMPLETE) {
      eventStr = get_event_string(event);
      dependStr = get_event_dependencies_string(event);
      XOCL_DEBUGF("KERNEL status: %d, event: %s, depend: %s\n", status, eventStr.c_str(), dependStr.c_str());
    }

    auto queue = event->get_command_queue();
    auto device = queue->get_device();
    auto commandState = event_status_to_profile_state(status);
    auto contextId =  event->get_context()->get_uid();
    auto commandQueueId = queue->get_uid();
    auto deviceName = device->get_name();
    auto deviceId = device->get_uid();
    double timestampMsec = 0.0;
    timestampMsec = (status == CL_COMPLETE) ? (event->time_end()) / 1e6 : timestampMsec;
    timestampMsec = (status == CL_RUNNING) ? (event->time_start()) / 1e6 : timestampMsec;
    // Create and insert trace string in xdp plugin
    std::string uniqueDeviceName = deviceName + "-" + std::to_string(deviceId);
    std::string localSize = std::to_string(localWorkDim[0]) + ":" +
                            std::to_string(localWorkDim[1]) + ":" +
                            std::to_string(localWorkDim[2]);
    std::string CuInfo = kname + "|" + localSize;
    std::string uniqueName = "KERNEL|" + uniqueDeviceName + "|" + xname + "|" + CuInfo + "|";
    std::string traceString = uniqueName + std::to_string(workGroupSize);
    OCLProfiler::Instance()->getPlugin()->setTraceStringForComputeUnit(kname, traceString);
    // Finally log the execution
    OCLProfiler::Instance()->getProfileManager()->logKernelExecution
      ( reinterpret_cast<uint64_t>(kernel)
       ,programId
       ,reinterpret_cast<uint64_t>(event)
       ,commandState
       ,kname
       ,xname
       ,contextId
       ,commandQueueId
       ,deviceName
       ,deviceId
       ,globalWorkDim
       ,workGroupSize
       ,localWorkDim
       ,cu_name
       ,eventStr
       ,dependStr
       ,timestampMsec);
}

void
cb_action_read(xocl::event* event,cl_int status, cl_mem buffer, size_t size, uint64_t address, const std::string& bank,
               bool entire_buffer, size_t user_size, size_t user_offset)
{
    if (!isProfilingOn())
      return;

    // Create string to specify event and its dependencies
    std::string eventStr;
    std::string dependStr;
    if (status == CL_RUNNING || status == CL_COMPLETE) {
      eventStr = get_event_string(event);
      dependStr = get_event_dependencies_string(event);
      XOCL_DEBUGF("READ status: %d, event: %s, depend: %s\n", status, eventStr.c_str(), dependStr.c_str());
    }

    // Catch if reading from P2P buffer
    auto ext_flags = xocl::xocl(buffer)->get_ext_flags();
    auto kind = (ext_flags & XCL_MEM_EXT_P2P_BUFFER) ? xdp::RTUtil::READ_BUFFER_P2P : xdp::RTUtil::READ_BUFFER;

    auto commandState = event_status_to_profile_state(status);
    auto queue = event->get_command_queue();
    auto deviceName = queue->get_device()->get_name();
    auto contextId =  event->get_context()->get_uid();
    auto numDevices = event->get_context()->num_devices();
    auto commandQueueId = event->get_command_queue()->get_uid();
    auto threadId = std::this_thread::get_id();
    double timestampMsec = (status == CL_COMPLETE) ? event->time_end() / 1e6 : 0.0;

    size_t actual_size = (entire_buffer) ? size : user_size;

    OCLProfiler::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(buffer)
       ,kind
       ,commandState
       ,actual_size
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,address
       ,bank
       ,address
       ,bank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);
}

void
cb_action_map(xocl::event* event,cl_int status, cl_mem buffer, size_t size, uint64_t address,
              const std::string& bank, cl_map_flags map_flags)
{
    if (!isProfilingOn())
      return;

    auto queue = event->get_command_queue();
    auto device = queue->get_device();

    // Ignore if invalidated region or buffer is *not* resident on device
    if ((map_flags & CL_MAP_WRITE_INVALIDATE_REGION) || !xocl::xocl(buffer)->is_resident(device))
      return;

    // Create string to specify event and its dependencies
    std::string eventStr;
    std::string dependStr;
    if (status == CL_RUNNING || status == CL_COMPLETE) {
      eventStr = get_event_string(event);
      dependStr = get_event_dependencies_string(event);
      XOCL_DEBUGF("MAP status: %d, event: %s, depend: %s\n", status, eventStr.c_str(), dependStr.c_str());
    }

    auto commandState = event_status_to_profile_state(status);
    auto deviceName = device->get_name();
    auto contextId =  event->get_context()->get_uid();
    auto numDevices = event->get_context()->num_devices();
    auto commandQueueId = event->get_command_queue()->get_uid();
    auto threadId = std::this_thread::get_id();
    double timestampMsec = (status == CL_COMPLETE) ? event->time_end() / 1e6 : 0.0;

    OCLProfiler::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(buffer)
       ,xdp::RTUtil::READ_BUFFER
       ,commandState
       ,size
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,address
       ,bank
       ,address
       ,bank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);
}

void
cb_action_write(xocl::event* event,cl_int status, cl_mem buffer, size_t size, uint64_t address, const std::string& bank,
                bool entire_buffer, size_t user_size, size_t user_offset)
{
    if (!isProfilingOn())
      return;

    auto queue = event->get_command_queue();
    auto device = queue->get_device();

    // Catch if buffer is *not* resident on device; if so, then covered by NDRange migration
    if (!xocl::xocl(buffer)->is_resident(device))
      return;

    // Create string to specify event and its dependencies
    std::string eventStr;
    std::string dependStr;
    if (status == CL_RUNNING || status == CL_COMPLETE) {
      eventStr = get_event_string(event);
      dependStr = get_event_dependencies_string(event);
      XOCL_DEBUGF("WRITE event: %s, depend: %s\n", eventStr.c_str(), dependStr.c_str());
    }

    // Catch if writing to P2P buffer
    auto ext_flags = xocl::xocl(buffer)->get_ext_flags();
    auto kind = (ext_flags & XCL_MEM_EXT_P2P_BUFFER) ? xdp::RTUtil::WRITE_BUFFER_P2P : xdp::RTUtil::WRITE_BUFFER;

    auto commandState = event_status_to_profile_state(status);
    auto deviceName = device->get_name();
    auto contextId =  event->get_context()->get_uid();
    auto numDevices = event->get_context()->num_devices();
    auto commandQueueId = event->get_command_queue()->get_uid();
    auto threadId = std::this_thread::get_id();
    double timestampMsec = (status == CL_COMPLETE) ? event->time_end() / 1e6 : 0.0;

    size_t actual_size = (entire_buffer) ? size : user_size;

    OCLProfiler::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(buffer)
       ,kind
       ,commandState
       ,actual_size
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,address
       ,bank
       ,address
       ,bank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);
}

void
cb_action_unmap (xocl::event* event,cl_int status, cl_mem buffer, size_t size, uint64_t address, const std::string& bank)
{
    if (!isProfilingOn())
      return;

    auto queue = event->get_command_queue();
    auto device = queue->get_device();

    // Catch if buffer is *not* resident on device (covered by NDRange migration) or is P2P buffer
    if (!xocl::xocl(buffer)->is_resident(device) || xocl::xocl(buffer)->is_p2p_memory())
      return;

    // Create string to specify event and its dependencies
    std::string eventStr;
    std::string dependStr;
    if (status == CL_RUNNING || status == CL_COMPLETE) {
      eventStr = get_event_string(event);
      dependStr = get_event_dependencies_string(event);
      XOCL_DEBUGF("UNMAP status: %d, event: %s, depend: %s\n", status, eventStr.c_str(), dependStr.c_str());
    }

    auto commandState = event_status_to_profile_state(status);
    auto deviceName = device->get_name();
    auto contextId =  event->get_context()->get_uid();
    auto numDevices = event->get_context()->num_devices();
    auto commandQueueId = event->get_command_queue()->get_uid();
    auto threadId = std::this_thread::get_id();
    double timestampMsec = (status == CL_COMPLETE) ? event->time_end() / 1e6 : 0.0;

    OCLProfiler::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(buffer)
       ,xdp::RTUtil::WRITE_BUFFER
       ,commandState
       ,size
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,address
       ,bank
       ,address
       ,bank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);

}

void
cb_action_ndrange_migrate (xocl::event* event,cl_int status, cl_mem mem0, size_t totalSize, uint64_t address, const std::string & bank)
{
    // Catch if there's nothing to migrate or profiling is off
    if (!isProfilingOn() || (totalSize == 0))
      return;

    // Create string to specify event and its dependencies
    std::string eventStr;
    std::string dependStr;
    if (status == CL_RUNNING || status == CL_COMPLETE) {
      eventStr = get_event_string(event);
      dependStr = get_event_dependencies_string(event);
      XOCL_DEBUGF("NDRANGE MIGRATE status: %d, event: %s, depend: %s, address: 0x%X, size: %d\n", status, eventStr.c_str(), dependStr.c_str(), address, totalSize);
    }

    auto commandState = event_status_to_profile_state(status);
    auto queue = event->get_command_queue();
    auto deviceName = queue->get_device()->get_name();
    auto contextId =  event->get_context()->get_uid();
    auto numDevices = event->get_context()->num_devices();
    auto commandQueueId = event->get_command_queue()->get_uid();
    auto threadId = std::this_thread::get_id();
    double timestampMsec = (status == CL_COMPLETE) ? event->time_end() / 1e6 : 0.0;

    OCLProfiler::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(mem0)
       ,xdp::RTUtil::WRITE_BUFFER
       ,commandState
       ,totalSize
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,address
       ,bank
       ,address
       ,bank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);
}

void
cb_action_migrate (xocl::event* event,cl_int status, cl_mem mem0, size_t totalSize, uint64_t address,
                   const std::string & bank, cl_mem_migration_flags flags)
{
    if (!isProfilingOn() || (flags & CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED) || (totalSize == 0))
      return;

    auto commandState = event_status_to_profile_state(status);

    // Create string to specify event and its dependencies
    std::string eventStr;
    std::string dependStr;
    if (status == CL_RUNNING || status == CL_COMPLETE) {
      eventStr = get_event_string(event);
      dependStr = get_event_dependencies_string(event);
      XOCL_DEBUGF("MIGRATE status: %d, event: %s, depend: %s, address: 0x%X, size: %d\n", status, eventStr.c_str(), dependStr.c_str(), address, totalSize);
    }

    auto queue = event->get_command_queue();
    auto deviceName = queue->get_device()->get_name();
    auto contextId =  event->get_context()->get_uid();
    auto numDevices = event->get_context()->num_devices();
    auto commandQueueId = event->get_command_queue()->get_uid();
    auto threadId = std::this_thread::get_id();
    xdp::RTUtil::e_profile_command_kind kind = (flags & CL_MIGRATE_MEM_OBJECT_HOST) ?
      xdp::RTUtil::READ_BUFFER : xdp::RTUtil::WRITE_BUFFER;
    double timestampMsec = (status == CL_COMPLETE) ? event->time_end() / 1e6 : 0.0;

    OCLProfiler::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(mem0)
       ,kind
       ,commandState
       ,totalSize
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,address
       ,bank
       ,address
       ,bank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);
}

void
cb_action_copy(xocl::event* event, cl_int status, cl_mem src_buffer, cl_mem dst_buffer,
               bool same_device, size_t size, uint64_t srcAddress, const std::string& srcBank,
               uint64_t dstAddress, const std::string& dstBank)
{
    if (!isProfilingOn())
      return;

    auto queue = event->get_command_queue();
    auto device = queue->get_device();

    // Create string to specify event and its dependencies
    std::string eventStr;
    std::string dependStr;
    if (status == CL_RUNNING || status == CL_COMPLETE) {
      eventStr = get_event_string(event);
      dependStr = get_event_dependencies_string(event);
      XOCL_DEBUGF("COPY event: %s, depend: %s\n", eventStr.c_str(), dependStr.c_str());
    }

    // Catch if copying to/from P2P buffer
    auto kind = (same_device) ? xdp::RTUtil::COPY_BUFFER : xdp::RTUtil::COPY_BUFFER_P2P;
    auto src_ext_flags = xocl::xocl(src_buffer)->get_ext_flags();
    auto dst_ext_flags = xocl::xocl(dst_buffer)->get_ext_flags();
    if ((src_ext_flags & XCL_MEM_EXT_P2P_BUFFER) || (dst_ext_flags & XCL_MEM_EXT_P2P_BUFFER))
      kind = xdp::RTUtil::COPY_BUFFER_P2P;

    auto commandState = event_status_to_profile_state(status);
    auto deviceName = device->get_name();
    auto contextId =  event->get_context()->get_uid();
    auto numDevices = event->get_context()->num_devices();
    auto commandQueueId = event->get_command_queue()->get_uid();
    auto threadId = std::this_thread::get_id();
    double timestampMsec = (status == CL_COMPLETE) ? event->time_end() / 1e6 : 0.0;

    OCLProfiler::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(src_buffer)
       ,kind
       ,commandState
       ,size
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,srcAddress
       ,srcBank
       ,dstAddress
       ,dstBank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);

}

void cb_log_function_start(const char* functionName, long long queueAddress, unsigned int functionID)
{
  OCLProfiler::Instance()->getProfileManager()->logFunctionCallStart(functionName, queueAddress, functionID);
}

void cb_log_function_end(const char* functionName, long long queueAddress, unsigned int functionID)
{
  OCLProfiler::Instance()->getProfileManager()->logFunctionCallEnd(functionName, queueAddress, functionID);
}

void cb_log_dependencies(xocl::event* event,  cl_uint num_deps, const cl_event* deps)
{
  if (!xrt::config::get_timeline_trace()) {
    return;
  }

  for (auto e :  xocl::get_range(deps, deps+num_deps)) {
    OCLProfiler::Instance()->getProfileManager()->logDependency(xdp::RTUtil::DEPENDENCY_EVENT,
                  xocl::xocl(e)->get_suid(), event->get_suid());
  }
}

void cb_add_to_active_devices(const std::string& device_name)
{
  auto profiler = OCLProfiler::Instance();
  static bool profile_on = profiler->applicationProfilingOn();
  if (profile_on) {
    profiler->addToActiveDevices(device_name);
    profiler->getPlugin()->setArgumentsBank(device_name);
  }
}

void
cb_set_kernel_clock_freq(const std::string& device_name, unsigned int freq)
{
  OCLProfiler::Instance()->setKernelClockFreqMHz(device_name, freq);
}

void cb_reset(const xocl::xclbin& xclbin)
{
  auto profiler = OCLProfiler::Instance();

  // init flow mode
  auto xclbin_target = xclbin.target();
  if (xclbin_target == xocl::xclbin::target_type::bin) {
    auto dsa = xclbin.dsa_name();
    // CR-964171: trace clock is 300 MHz on DDR4 systems (e.g., KU115 4DDR)
    // TODO: this is kludgy; replace this with getting info from feature ROM
    // http://confluence.xilinx.com/display/XIP/DSA+Feature+ROM+Proposal
    if(dsa.find("4ddr") != std::string::npos)
      profiler->getProfileManager()->setDeviceTraceClockFreqMHz(300.0);
    profiler->getPlugin()->setFlowMode(xdp::RTUtil::DEVICE);
  } else if (xclbin_target == xocl::xclbin::target_type::csim) {
    profiler->getPlugin()->setFlowMode(xdp::RTUtil::CPU);
    // old and unsupported modes
    profiler->turnOffProfile(xdp::RTUtil::PROFILE_DEVICE);
  } else if (xclbin_target == xocl::xclbin::target_type::cosim) {
    profiler->getPlugin()->setFlowMode(xdp::RTUtil::COSIM_EM);
    profiler->turnOffProfile(xdp::RTUtil::PROFILE_DEVICE);
  } else if (xclbin_target == xocl::xclbin::target_type::hwem) {
    profiler->getPlugin()->setFlowMode(xdp::RTUtil::HW_EM);
  } else if (xclbin_target == xocl::xclbin::target_type::x86) {
  } else if (xclbin_target == xocl::xclbin::target_type::zynqps7) {
  } else {
    throw xocl::error(CL_INVALID_BINARY,"invalid xclbin region target");
  }
}

void
cb_init()
{
  xdp::RTSingleton::Instance()->getStatus();
}

void register_xocl_profile_callbacks() {
  xocl::profile::register_cb_action_read (cb_action_read);
  xocl::profile::register_cb_action_write (cb_action_write);
  xocl::profile::register_cb_action_map (cb_action_map);
  xocl::profile::register_cb_action_migrate (cb_action_migrate);
  xocl::profile::register_cb_action_ndrange_migrate (cb_action_ndrange_migrate);
  xocl::profile::register_cb_action_ndrange (cb_action_ndrange);
  xocl::profile::register_cb_action_unmap (cb_action_unmap);
  xocl::profile::register_cb_action_copy (cb_action_copy);

  xocl::profile::register_cb_log_function_start(cb_log_function_start);
  xocl::profile::register_cb_log_function_end(cb_log_function_end);
  xocl::profile::register_cb_log_dependencies(cb_log_dependencies);
  xocl::profile::register_cb_add_to_active_devices(cb_add_to_active_devices);
  xocl::profile::register_cb_set_kernel_clock_freq(cb_set_kernel_clock_freq);
  xocl::profile::register_cb_reset(cb_reset);
  xocl::profile::register_cb_init(cb_init);

  xocl::profile::register_cb_get_device_trace(cb_get_device_trace);
  xocl::profile::register_cb_get_device_counters(cb_get_device_counters);
  xocl::profile::register_cb_start_device_profiling(cb_start_device_profiling);
  xocl::profile::register_cb_reset_device_profiling(cb_reset_device_profiling);
  xocl::profile::register_cb_end_device_profiling(cb_end_device_profiling);
}
} // xdp

extern "C"
void
initXDPLib()
{
  try {
    (void)xdp::RTSingleton::Instance();
    (void)xdp::OCLProfiler::Instance();
  } catch (std::runtime_error& e) {
    xrt::message::send(xrt::message::severity_level::XRT_WARNING, e.what());
    // Don't register any of the callbacks.  Something went wrong during
    //  initialization.
    return ;
  }

  if (xdp::OCLProfiler::Instance()->applicationProfilingOn())
    xdp::register_xocl_profile_callbacks();
}
