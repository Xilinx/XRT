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
 * This file contains the API for adapting the mixed xcl/xocl
 * data structures to the profiling infrastructure.
 *
 * Once xcl has been eliminated, this file should move to xocl/core
 * Temporarily, the file abuses the xocl namespace, but it cannot
 * currently be moved since profile.cpp has xcl dependencies that
 * are strictly forbidden in xocl.
 */

#include "profile.h"
#include "xdp/rt_singleton.h"
#include "xdp/profile/rt_profile.h"
#include "xdp/profile/rt_profile_xocl.h"

#include "xocl/core/event.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/device.h"
#include "xocl/core/kernel.h"
#include "xocl/core/context.h"
#include "xocl/core/program.h"
#include "xocl/core/range.h"
#include "xocl/core/execution_context.h"

#include "xocl/xclbin/xclbin.h"

#include <map>
#include <sstream>

namespace {

// Global flag that remains valid during shutdown
static bool s_exiting = false;

// Exiter that is constructed after (=> destructured before)
// critical data structures that should disable profiling.
struct X
{
  ~X() { s_exiting = true; }
};

static XCL::RTProfile::e_profile_command_state
event_status_to_profile_state(cl_int status) 
{
  static const std::map<cl_int, XCL::RTProfile::e_profile_command_state> tbl 
  {
    {CL_QUEUED,    XCL::RTProfile::QUEUE}
   ,{CL_SUBMITTED, XCL::RTProfile::SUBMIT}
   ,{CL_RUNNING,   XCL::RTProfile::START}
   ,{CL_COMPLETE,  XCL::RTProfile::END}
  };

  static X x;

  auto itr = tbl.find(status);
  if (itr==tbl.end())
    throw std::runtime_error("bad event status '" + std::to_string(status) + "'");
  return (*itr).second;
}

} // namespace

namespace xocl { namespace profile {

bool isProfilingOn() {
  //static bool profilingOn = xdp::profile::isApplicationProfilingOn();
  //return profilingOn;
  return xdp::profile::isApplicationProfilingOn();
}

void
log(xocl::event* event, cl_int status)
{
  if (!s_exiting)
    event->trigger_profile_action(status,"");
}

void
log(xocl::event* event, cl_int status, const std::string& cuname)
{
  if (!s_exiting)
    event->trigger_profile_action(status,cuname);
}

void
log_dependencies (xocl::event* event,  cl_uint num_deps, const cl_event* deps) {

  if (!xrt::config::get_timeline_trace()) {
    return;
  }

  for (auto e :  xocl::get_range(deps, deps+num_deps)) {
    XCL::RTSingleton::Instance()->getProfileManager()->logDependency(XCL::RTProfile::DEPENDENCY_EVENT,
                  xocl::xocl(e)->get_suid(), event->get_suid());
  }
}

// Attempt to get DDR physical address and bank number
void get_address_bank(cl_mem buffer, uint64_t &address, std::string &bank)
{
  try {
    auto xoclMem = xocl::xocl(buffer);
    xoclMem->try_get_address_bank(address, bank);
    return;
  }
  catch (const xocl::error& ex) {
    // cannot get address/bank
    address = 0;
    bank = "Unknown";
  }
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

xocl::event::action_profile_type
action_ndrange(cl_event event, cl_kernel kernel)
{
  // profile action is invoked after the event is marked complete and 
  // at that time the kenel may have been released by a subsequent
  // clReleaseKernel.
  auto exctx = xocl::xocl(event)->get_execution_context();
  auto workGroupSize = xocl::xocl(kernel)->get_wg_size();
  auto globalWorkDim = exctx->get_global_work_size();
  size_t localWorkDim[3] = {0,0,0};
  range_copy(xocl::xocl(kernel)->get_compile_wg_size_range(),localWorkDim);
  if (localWorkDim[0] == 0 && localWorkDim[1] == 0 && localWorkDim[2] == 0) {
    std::copy(exctx->get_local_work_size(),exctx->get_local_work_size()+3,localWorkDim);
  }

  // Leg work to access the xclbin project name.  The device may have
  // been reloaded with a new binary by the time the action itself is
  // called, so the work has be done here.
  auto device = xocl::xocl(event)->get_command_queue()->get_device();
  auto program = xocl::xocl(kernel)->get_program();
  auto programId = xocl::xocl(kernel)->get_program()->get_uid();
  auto xclbin = program->get_xclbin(device);

  std::string xname = xclbin.project_name();
  std::string kname  = xocl::xocl(kernel)->get_name();

  return [kernel,kname,xname,workGroupSize,globalWorkDim,localWorkDim,programId](xocl::event* event,cl_int status,const std::string& cu_name) {
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
    XCL::RTSingleton::Instance()->getProfileManager()->logKernelExecution
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
  };
}

xocl::event::action_profile_type
action_read(cl_mem buffer)
{
  std::string bank;
  uint64_t address;
  get_address_bank(buffer, address, bank);
  auto size = xocl::xocl(buffer)->get_size();

  return [buffer,size,address,bank](xocl::event* event,cl_int status, const std::string&) {
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

    auto commandState = event_status_to_profile_state(status);
    auto queue = event->get_command_queue();
    auto deviceName = queue->get_device()->get_name();
    auto contextId =  event->get_context()->get_uid();
    auto numDevices = event->get_context()->num_devices();
    auto commandQueueId = event->get_command_queue()->get_uid();
    auto threadId = std::this_thread::get_id();
    double timestampMsec = (status == CL_COMPLETE) ? event->time_end() / 1e6 : 0.0;

    XCL::RTSingleton::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(buffer)
       ,XCL::RTProfile::READ_BUFFER
       ,commandState
       ,size
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,address
       ,bank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);
  };
}

xocl::event::action_profile_type
action_map(cl_mem buffer,cl_map_flags map_flags)
{
  std::string bank;
  uint64_t address;
  get_address_bank(buffer, address, bank);
  auto size = xocl::xocl(buffer)->get_size();

  return [buffer,size,address,bank,map_flags](xocl::event* event,cl_int status,const std::string&) {
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

    XCL::RTSingleton::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(buffer)
       ,XCL::RTProfile::READ_BUFFER
       ,commandState
       ,size
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,address
       ,bank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);
  };
}

xocl::event::action_profile_type
action_write(cl_mem buffer)
{
  std::string bank;
  uint64_t address;
  get_address_bank(buffer, address, bank);
  auto size = xocl::xocl(buffer)->get_size();

  return [buffer,size,address,bank](xocl::event* event,cl_int status,const std::string&) {
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

    auto commandState = event_status_to_profile_state(status);
    auto deviceName = device->get_name();
    auto contextId =  event->get_context()->get_uid();
    auto numDevices = event->get_context()->num_devices();
    auto commandQueueId = event->get_command_queue()->get_uid();
    auto threadId = std::this_thread::get_id();
    double timestampMsec = (status == CL_COMPLETE) ? event->time_end() / 1e6 : 0.0;

    XCL::RTSingleton::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(buffer)
       ,XCL::RTProfile::WRITE_BUFFER
       ,commandState
       ,size
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,address
       ,bank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);
  };
}

xocl::event::action_profile_type
action_unmap(cl_mem buffer)
{
  std::string bank;
  uint64_t address;
  get_address_bank(buffer, address, bank);
  auto size = xocl::xocl(buffer)->get_size();

  return [buffer,size,address,bank](xocl::event* event,cl_int status,const std::string&) {
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
      XOCL_DEBUGF("UNMAP status: %d, event: %s, depend: %s\n", status, eventStr.c_str(), dependStr.c_str());
    }

    auto commandState = event_status_to_profile_state(status);
    auto deviceName = device->get_name();
    auto contextId =  event->get_context()->get_uid();
    auto numDevices = event->get_context()->num_devices();
    auto commandQueueId = event->get_command_queue()->get_uid();
    auto threadId = std::this_thread::get_id();
    double timestampMsec = (status == CL_COMPLETE) ? event->time_end() / 1e6 : 0.0;

    XCL::RTSingleton::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(buffer)
       ,XCL::RTProfile::WRITE_BUFFER
       ,commandState
       ,size
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,address
       ,bank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);
  };
}

xocl::event::action_profile_type
action_ndrange_migrate(cl_event event, cl_kernel kernel)
{
  cl_mem mem0 = nullptr;
  std::string bank = "Unknown";
  uint64_t address = 0;
  size_t totalSize = 0;

  auto command_queue = xocl::xocl(event)->get_command_queue();
  auto device = command_queue->get_device();

  // Calculate total size and grab first address & bank
  // NOTE: argument must be: NOT a progvar, NOT write only, and NOT resident
  for (auto& arg : xocl::xocl(kernel)->get_argument_range()) {
    if (auto mem = arg->get_memory_object()) {
      if (arg->is_progvar() && arg->get_address_qualifier()==CL_KERNEL_ARG_ADDRESS_GLOBAL)
        // DO NOTHING: progvars are not transfered
        continue;
      else if (mem->is_resident(device))
        continue;
      else if (!(mem->get_flags() & (CL_MEM_WRITE_ONLY|CL_MEM_HOST_NO_ACCESS))) {
        if (totalSize == 0) {
          mem0 = mem;
          get_address_bank(mem, address, bank);
        }

        totalSize += xocl::xocl(mem)->get_size();
      }
    }
  }

  return [mem0,totalSize,address,bank](xocl::event* event,cl_int status,const std::string&) {
    // Catch if there's nothing to migrate or profiling is off
    if (!isProfilingOn() || (totalSize == 0))
      return;

    //Fix for CR-1004188 Using of the static variable below does not work
    //as intended. There is only one copy of migrateCount created for all ndrange migrate events
    //We need to address following cases:
    //1. For the case when no buffers are migrated by ndrange migrate
    //   ==> we will not receive "running" but we will receive compelete, there will be an "END" entry in csv file
    //       without corresponding "START" entry. sdx_analyze should drop the unmatched END
    //2. For the case when n out of m buffers are migrated (n<=m)
    //   ==> we will receive one callback for "running" and one callback for "complete", ie. we produce matching
    //       start and end
#if 0
    // Ensure there are START/END pairs
    // NOTE: When clEnqueueMigrateMemObjects is used, END events are received here, too
    static int migrateCount = 0;
    if (status == CL_RUNNING) {
      migrateCount++;
    }
    else if (status == CL_COMPLETE) {
      if (migrateCount > 0)
        migrateCount--;
      else
        return;
    }
#endif

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

    XCL::RTSingleton::Instance()->getProfileManager()->logDataTransfer
      (reinterpret_cast<uint64_t>(mem0)
       ,XCL::RTProfile::WRITE_BUFFER
       ,commandState
       ,totalSize
       ,contextId
       ,numDevices
       ,deviceName
       ,commandQueueId
       ,address
       ,bank
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);
  };
}

xocl::event::action_profile_type
action_migrate(cl_uint num_mem_objects, const cl_mem *mem_objects, cl_mem_migration_flags flags)
{
  // profile action is invoked after the event is marked complete and 
  // at that time the buffer may have been released by a subsequent
  // clReleaseMemObject
  cl_mem mem0 = (num_mem_objects > 0) ? mem_objects[0] : nullptr;

  std::string bank;
  uint64_t address;
  get_address_bank(mem0, address, bank);

  size_t totalSize = 0;
  for (auto mem : xocl::get_range(mem_objects,mem_objects+num_mem_objects)) {
    totalSize += xocl::xocl(mem)->get_size();
  }

  return [mem0,totalSize,address,bank,flags](xocl::event* event,cl_int status,const std::string&) {
    if (!isProfilingOn() || (flags & CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED) || (totalSize == 0))
      return;

    auto commandState = event_status_to_profile_state(status);

#if 0
    // Report the first START and the last END
    static int numOutstanding = 0;
    if (commandState == XCL::RTProfile::SUBMIT) {
      numOutstanding = 0;
    }
    else if (commandState == XCL::RTProfile::START) {
   	  numOutstanding++;
      XOCL_DEBUGF("action_migrate: START, outstanding = %d\n", numOutstanding);
   	  //if (numOutstanding != 1)
   	  //  return;
    }
    else if (commandState == XCL::RTProfile::END) {
   	  numOutstanding--;
      XOCL_DEBUGF("action_migrate: END, outstanding = %d\n", numOutstanding);
   	  //if (numOutstanding != 0)
   	  //  return;
    }
#endif

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
    XCL::RTProfile::e_profile_command_kind kind = (flags & CL_MIGRATE_MEM_OBJECT_HOST) ?
      XCL::RTProfile::READ_BUFFER : XCL::RTProfile::WRITE_BUFFER;
    double timestampMsec = (status == CL_COMPLETE) ? event->time_end() / 1e6 : 0.0;

    XCL::RTSingleton::Instance()->getProfileManager()->logDataTransfer
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
       ,threadId
       ,eventStr
       ,dependStr
       ,timestampMsec);
  };

}

function_call_logger::
function_call_logger(const char* function)
  : function_call_logger(function,0)
{}
  
function_call_logger::
function_call_logger(const char* function, long long address)
  : m_name(function), m_address(address)
{
  static bool profile_on = XCL::RTSingleton::Instance()->applicationProfilingOn();
  if (profile_on && XCL::active())
    XCL::RTSingleton::Instance()->getProfileManager()->logFunctionCallStart(m_name, m_address);
}

function_call_logger::
~function_call_logger()
{
  static bool profile_on = XCL::RTSingleton::Instance()->applicationProfilingOn();
  if (profile_on && XCL::active())
    XCL::RTSingleton::Instance()->getProfileManager()->logFunctionCallEnd(m_name, m_address);
}

void
add_to_active_devices(const std::string& device_name)
{
  static bool profile_on = XCL::RTSingleton::Instance()->applicationProfilingOn();
  if (profile_on)
    XCL::RTSingleton::Instance()->getProfileManager()->addToActiveDevices(device_name);
}

void
set_kernel_clock_freq(const std::string& device_name, unsigned int freq)
{
  static bool profile_on = XCL::RTSingleton::Instance()->applicationProfilingOn();
  if (profile_on)
    XCL::RTSingleton::Instance()->getProfileManager()->setKernelClockFreqMHz(device_name, freq);
}

void
reset(const xocl::xclbin& xclbin)
{
  auto rts = XCL::RTSingleton::Instance();

  // init profilers
  // NOTE: now performed with debug_ip_layout
#if 0
  for (auto& profiler : xclbin.profilers()) {
    if (profiler.name.find("monitor_kernels") != std::string::npos
        || profiler.name.find("monitor_stalls") != std::string::npos) {
      rts->setOclProfileSlots(profiler.slots.size());
      for (auto& slot : profiler.slots) {
        rts->getProfileManager()->setSlotComputeUnitName(std::get<0>(slot), std::get<1>(slot));
        rts->setOclProfileMode(std::get<0>(slot), std::get<2>(slot));
      }
    }
  }
#endif

  // init flow mode
  auto xclbin_target = xclbin.target();
  if (xclbin_target == xocl::xclbin::target_type::bin) {
    auto dsa = xclbin.dsa_name();
    // CR-964171: trace clock is 300 MHz on DDR4 systems (e.g., KU115 4DDR)
    // TODO: this is kludgy; replace this with getting info from feature ROM
    // http://confluence.xilinx.com/display/XIP/DSA+Feature+ROM+Proposal
    if(dsa.find("4ddr") != std::string::npos)
      rts->getProfileManager()->setDeviceTraceClockFreqMHz(300.0);
    rts->setFlowMode(XCL::RTSingleton::DEVICE);
  } else if (xclbin_target == xocl::xclbin::target_type::csim) {
    rts->setFlowMode(XCL::RTSingleton::CPU);
  } else if (xclbin_target == xocl::xclbin::target_type::cosim) {
    rts->setFlowMode(XCL::RTSingleton::COSIM_EM);
  } else if (xclbin_target == xocl::xclbin::target_type::hwem) {
    rts->setFlowMode(XCL::RTSingleton::HW_EM);
  } else if (xclbin_target == xocl::xclbin::target_type::x86) {
  } else if (xclbin_target == xocl::xclbin::target_type::zynqps7) {
  } else {
    throw xocl::error(CL_INVALID_BINARY,"invalid xclbin region target");
  }
}

void
init()
{
  XCL::RTSingleton::Instance()->getStatus();
}

}}


