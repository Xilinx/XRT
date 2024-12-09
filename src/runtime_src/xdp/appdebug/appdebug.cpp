/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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
 * This file contains the implementation of the application debug
 * It exposes a set of functions that are callable from debugger(GDB)
 * It defines data structures that provide the view of runtime data structures such as cl_event and cl_meme
 * It defines lambda functions that are attached as debug action with the cl_event
 */

#define XDP_PLUGIN_SOURCE

#include "xdp/appdebug/appdebug.h"
#include "xdp/appdebug/appdebug_track.h"
#include "xdp/appdebug/appdebug_plugin.h"

#include "xocl/core/event.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/device.h"
#include "xocl/core/platform.h"
#include "xocl/core/context.h"
#include "xocl/core/compute_unit.h"
#include "xocl/core/execution_context.h"

#include "core/include/xdp/app_debug.h"
#include "core/include/xdp/axi_checker_codes.h"
#include "core/include/xdp/common.h"
#include "core/include/xdp/counters.h"
#include "core/include/xrt/detail/xclbin.h"

#include <map>
#include <sstream>
#include <fstream>
#include <limits>
#include "xocl/api/plugin/xdp/appdebug.h"

#ifdef _WIN32
#pragma warning (disable : 4996)
/* Disable warning for use of getenv */
#endif

namespace {
static const int debug_ip_layout_max_size = 65536;

const char*
event_commandtype_to_string(cl_command_type cmd)
{
  if (cmd < CL_COMMAND_NDRANGE_KERNEL || cmd > CL_COMMAND_FILL_IMAGE) {
    return "Bad command";
  }
  static const char* tbl [] = {"CL_COMMAND_NDRANGE_KERNEL", "CL_COMMAND_TASK", "CL_COMMAND_NATIVE_KERNEL", "CL_COMMAND_READ_BUFFER", "CL_COMMAND_WRITE_BUFFER",
                               "CL_COMMAND_COPY_BUFFER", "CL_COMMAND_READ_IMAGE", "CL_COMMAND_WRITE_IMAGE", "CL_COMMAND_COPY_IMAGE", "CL_COMMAND_COPY_IMAGE_TO_BUFFER",
                               "CL_COMMAND_COPY_BUFFER_TO_IMAGE", "CL_COMMAND_MAP_BUFFER", "CL_COMMAND_MAP_IMAGE", "CL_COMMAND_UNMAP_MEM_OBJECT", "CL_COMMAND_MARKER",
                               "CL_COMMAND_ACQUIRE_GL_OBJECTS", "CL_COMMAND_RELEASE_GL_OBJECTS", "CL_COMMAND_READ_BUFFER_RECT", "CL_COMMAND_WRITE_BUFFER_RECT", "CL_COMMAND_COPY_BUFFER_RECT",
                               "CL_COMMAND_USER", "CL_COMMAND_BARRIER", "CL_COMMAND_MIGRATE_MEM_OBJECTS", "CL_COMMAND_FILL_BUFFER", "CL_COMMAND_FILL_IMAGE"};
  return tbl[cmd-CL_COMMAND_NDRANGE_KERNEL];
}
const char*
event_commandstatus_to_string(cl_int status)
{
  if (status == -1) return "Locked";
  if (status > 3 || status < 0)return "Unknown";
  static const char* tbl[] = {"Complete", "Running","Submitted","Queued"};
  return tbl[status];
}

//Iterate all events and find all events that aEvent depends on, returns a vector
//Note that, this function calls try_get_chain() which locks the event object
//So any functions called while iterating on the chain should not lock the event
//Also, note that app_debug_track->for_each locks the tracker data structure,
//so the lambda cannot call any functions that would inturn try to lock tracker
std::vector<xocl::event*> event_chain_to_dependencies (xocl::event* aEvent) {
  std::vector<xocl::event*> dependencies;

  auto findDependencies = [aEvent, &dependencies] (cl_event aEv) {
    xocl::event * e =  xocl::xocl(aEv);
    //consider all events, including user events that are not in any command queue
      xocl::range_lock<xocl::event::event_iterator_type>&& aRange = e->try_get_chain();
      for (auto it = aRange.begin(); it!=aRange.end(); ++it) {
        if (*it == aEvent) {
          //Add ev to the aEvent dependent list
          dependencies.push_back(e);
          break;
        }
      }
  };

  appdebug::app_debug_track<cl_event>::getInstance()->for_each(findDependencies);
  return dependencies;
}

std::string event_dependencies_to_string(std::vector<xocl::event*>&& dependencies ) {
  std::stringstream sstr;
  if (!dependencies.size())
    return "None";
  for(auto it = dependencies.begin(); it!=dependencies.end(); ++it) {
    xocl::event* e = *it;
    std::string status_str;
    try {
      status_str = event_commandstatus_to_string(e->try_get_status());
    }
    catch (const xocl::error & ) {
      status_str = "Not Available";
    }
    sstr << "[" << std::hex << (cl_event)e << ", " << std::dec << e->get_uid() << ", " << status_str << ", " <<
      event_commandtype_to_string (e->get_command_type()) << "]";
  }
  return sstr.str();
}

} // namespace

namespace appdebug {

// Call back function to be called when a command is sent to the scheduler
void cb_scheduler_cmd_start (const xocl::execution_context* aContext, const xrt::run&)
{
  //update the datastructure associated with the given event
  try {
    app_debug_track<cl_event>::event_data_t& edt = app_debug_track<cl_event>::getInstance()->get_data(static_cast<cl_event>(const_cast<xocl::event*>(aContext->get_event())));
    edt.m_start = true;
  }
  catch (const xocl::error & ex) {
    //INVALID_OBJECT is the only expected exception, anything else rethrow
    if(ex.get_code() != DBG_EXCEPT_INVALID_OBJECT) {
      throw;
    }
  }
}



// Call back function to be called when a command is finished
void cb_scheduler_cmd_done (const xocl::execution_context* aContext, const xrt::run&)
{
  //update the datastructure associated with the given event
  try {
    app_debug_track<cl_event>::event_data_t& edt = app_debug_track<cl_event>::getInstance()->get_data(static_cast<cl_event>(const_cast<xocl::event*>(aContext->get_event())));
    ++edt.m_ncomplete;
  }
  catch (const xocl::error & ex) {
    //INVALID_OBJECT is the only expected exception, anything else rethrow
    if(ex.get_code() != DBG_EXCEPT_INVALID_OBJECT) {
      throw;
    }
  }
}

bool app_debug_view_base::isInValid() const
{
  return m_invalid;
}

std::string app_debug_view_base::geterrmsg() const
{
  return m_msg;

}

template<typename T>
T* app_debug_view<T> :: getdata() const
{
  return m_data;
}
/*
 * create explicit instantiation of template classes
 * This is needed as there are no other instantiation
 * created in the code and for gdb to call these
 * we need template instantiation of these
 */
template class app_debug_view<std::pair<size_t,size_t>>;
template class app_debug_view<std::vector<event_debug_view_base*>>;
template class app_debug_view<clmem_debug_view>;
template class app_debug_view<event_debug_view_base>;
template class app_debug_view<std::vector<kernel_debug_view*>>;
template class app_debug_view<aim_debug_view>;
template class app_debug_view<asm_debug_view>;
template class app_debug_view<am_debug_view>;
template class app_debug_view<lapc_debug_view>;
template class app_debug_view<std::vector<cl_command_queue>>;
template class app_debug_view<std::vector<cl_mem>>;

//Initialize the static member of app_debug_track
template<> bool app_debug_track<cl_command_queue>::m_set = true;
template<> bool app_debug_track<cl_mem>::m_set = true;
bool app_debug_track<cl_event>::m_set = true;

template class app_debug_track<cl_command_queue>;
template class app_debug_track<cl_mem>;



std::string event_debug_view_base::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;
  std::string quotes;
  if (aQuotes) quotes = "\"";
  else  quotes = "";

  //for the xpe commands show more info about commands
  if (aVerbose) {
    if (m_event) {
      sstr << quotes << "Event" << quotes << " : " << quotes << std::hex <<  m_event << quotes << ", ";
      if (xocl::xocl(m_event)->get_command_queue()) {
        sstr << quotes << "Queue" << quotes << " : " << quotes << std::hex << xocl::xocl(m_event)->get_command_queue() << quotes << ", ";
        if (xocl::xocl(m_event)->get_command_queue()->get_device()) {
          sstr << quotes << "Device" << quotes << " : " << quotes << xocl::xocl(m_event)->get_command_queue()->get_device()->get_name() << quotes << ", ";
        }
      }
      else {
        sstr << quotes << "Queue" << quotes << " : " << quotes << "None" << quotes << ", ";
      }
    }
  }
  if (aQuotes) {
    //For gui dbg perspective we want to show event pointers
    sstr << quotes << "name" << quotes << " : " << quotes  << "Event-" << std::hex << m_event << quotes << ", ";
  }
  sstr << quotes << "Uid" << quotes << " : " << quotes << std::dec << m_uid << quotes << ", ";
  sstr << quotes << "Status" << quotes << " : " << quotes << m_status_name << quotes << ", ";
  sstr << quotes << "Type" << quotes << " : " << quotes << m_command_name << quotes << ", ";
  sstr << quotes << "WaitingOn" << quotes << " : " << quotes << m_wait_list << quotes ;
  return sstr.str();
}

std::string event_debug_view_readwrite::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;
  std::string quotes;
  if (aQuotes) quotes = "\"";
  else  quotes = "";

  sstr << event_debug_view_base::getstring(aVerbose, aQuotes) << ", ";
  sstr << quotes << "Description" << quotes << " : ";
  sstr << quotes;
  sstr << "Transfer " << m_size << " bytes " << ((m_cmd == CL_COMMAND_READ_BUFFER)? "from " : "to ");
  sstr <<  "cl_mem " << std::hex << m_buffer << "+" << std::dec << m_offset;
  sstr << quotes;
  return sstr.str();
}
std::string event_debug_view_copy::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;
  std::string quotes;
  if (aQuotes) quotes = "\"";
  else  quotes = "";

  sstr << event_debug_view_base::getstring(aVerbose, aQuotes) << ", ";
  sstr << quotes << "Description" << quotes << " : ";
  sstr << quotes;
  sstr << "Copy from ";
  sstr <<  "cl_mem " << std::hex << m_src_buffer << "+" << std::dec << m_src_offset;
  sstr << " to ";
  sstr << " cl_mem " << std::hex << m_dst_buffer << "+" << std::dec << m_dst_offset;
  sstr << quotes;
  return sstr.str();
}
std::string event_debug_view_fill::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;
  std::string quotes;
  if (aQuotes) quotes = "\"";
  else  quotes = "";
  sstr << event_debug_view_base::getstring(aVerbose, aQuotes) << ", ";
  sstr << quotes << "Description" << quotes << " : ";
  sstr << quotes;
  sstr << "Fill " << m_size << " bytes into";
  sstr << " cl_mem " << std::hex << m_buffer << "+" << std::dec << m_offset;
  sstr << " with " << m_pattern_size << " bytes of " << std::hex << m_pattern ;
  sstr << quotes;
  return sstr.str();
}
std::string event_debug_view_map::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;
  std::string quotes;
  if (aQuotes) quotes = "\"";
  else  quotes = "";

  sstr << event_debug_view_base::getstring(aVerbose, aQuotes) << ", ";
  sstr << quotes << "Description" << quotes << " : ";
  sstr << quotes;
  sstr <<  "Map cl_mem " << std::hex << m_buffer << " with flags " << "0x" << std::hex << m_flags;
  sstr << quotes;
  return sstr.str();
}
std::string event_debug_view_migrate::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;
  std::string quotes;
  if (aQuotes) quotes = "\"";
  else  quotes = "";

  sstr << event_debug_view_base::getstring(aVerbose, aQuotes) << ", ";
  sstr << quotes << "Description" << quotes << " : ";
  sstr << quotes;
  if (m_kernel_args_migrate) {
    sstr << "Migrate kernel args for " << m_kname;
  }
  else {
    sstr << "Migrate " << m_num_objects << " cl_mem objects ";
    for(unsigned int i = 0; i<m_num_objects; ++i)
      sstr << std::hex << m_mem_objects[i] << " ";
    sstr << " with flags " << m_flags;
  }
  sstr << quotes;
  return sstr.str();
}
std::string event_debug_view_ndrange::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;
  std::string quotes;
  std::string total_workgroups;
  std::string completed_workgroups;

  if (aQuotes) {
    quotes = "\"";
    total_workgroups = "TotalWorkGroups";
    completed_workgroups = "CompletedWorkGroups";
  }
  else {
    quotes = "";
    total_workgroups = "Total WorkGroups";
    completed_workgroups = "Completed WorkGroups";
  }
  sstr << event_debug_view_base::getstring(aVerbose, aQuotes) << ", ";
  sstr << quotes << "KernelName" << quotes << " : ";
  sstr << quotes << m_kname << quotes << ", ";
  if (m_submitted) {
    sstr << quotes << total_workgroups << quotes << " : " << quotes << m_nworkgroups << quotes << ", ";
    sstr << quotes << completed_workgroups << quotes << " : " << quotes << m_ncompleted << quotes;
    //sstr << quotes << "Workgroups" << quotes << " : ";
    //sstr << quotes << m_ncompleted << "/" << m_nworkgroups << " completed";
  }
  else {
    sstr << quotes << total_workgroups << quotes << " : " << quotes << m_nworkgroups << quotes << ", ";
    sstr << quotes << completed_workgroups << quotes << " : " << quotes << "None" << quotes;
    //sstr << quotes << "Workgroups" << quotes << " : ";
    //sstr << quotes << "None scheduled";
  }
  return sstr.str();
}
std::string event_debug_view_unmap::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;
  std::string quotes;
  if (aQuotes) quotes = "\"";
  else  quotes = "";

  sstr << event_debug_view_base::getstring(aVerbose) << ", ";

  sstr << quotes << "Description" << quotes << " : ";
  sstr << quotes;
  sstr << "Unmap cl_mem " << std::hex << m_buffer;
  sstr << quotes;
  return sstr.str();
}

std::string event_debug_view_barrier_marker::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;

  std::string quotes;
  if (aQuotes) quotes = "\"";
  else  quotes = "";

  sstr << event_debug_view_base::getstring(aVerbose) << ", ";
  sstr << quotes << "Description" << quotes << " : ";
  sstr << quotes;
  sstr << "Wait for events in dependency list to complete";
  sstr << quotes;
  return sstr.str();
}

std::string event_debug_view_readwrite_image::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;
  std::string quotes;
  if (aQuotes) quotes = "\"";
  else  quotes = "";

  sstr << event_debug_view_base::getstring(aVerbose) << ", ";
  sstr << quotes << "Description" << quotes << " : ";
  sstr << quotes;
  sstr << "Read image " << ((m_cmd == CL_COMMAND_READ_IMAGE)? "from " : "to ");
  sstr <<  "cl_mem " << std::hex << m_image << " row-pitch: " << std::dec << m_row_pitch << " slice-pitch: " << m_slice_pitch;
  sstr << ", origin: (" << m_origin[0] << "," << m_origin[1] << "," << m_origin[2] << ")";
  sstr << ", region: (" << m_region[0] << "," << m_region[1] << "," << m_region[2] << ")";
  sstr << quotes;
  return sstr.str();
}

std::string clmem_debug_view::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;
  std::string quotes;
  if (aQuotes) quotes = "\"";
  else  quotes = "";

  sstr << quotes << "Mem" << quotes << " : " << quotes << std::hex <<  m_mem << quotes << ", ";
  sstr << quotes << "MemID" << quotes << " : " << quotes << std::dec << m_uid << quotes << ", ";
  sstr << quotes << "Device Memory Address" << quotes << " : " << quotes << "0x" << std::hex << m_device_addr << quotes << ", ";
  if (!m_bank.empty()) {
    sstr << quotes << "Bank" << quotes << " : " << quotes << std::dec << m_bank << quotes << ", ";
  }
  else {
    sstr << quotes << "Bank" << quotes << " : " << quotes << "Unavailable" << quotes << ", ";
  }
  sstr << quotes << "Size" << quotes << " : " << quotes << std::dec << m_size << quotes << ", ";
  sstr << quotes << "HostAddress" << quotes << " : " << quotes << std::hex << m_host_addr << quotes ;

  return sstr.str();
}

std::string kernel_debug_view::getstring(int aVerbose, int aQuotes)
{
  std::stringstream sstr;
  std::string quotes;
  if (aQuotes) quotes = "\"";
  else  quotes = "";

  sstr << quotes << "Kernel" << quotes << " : " << quotes << m_kname << quotes << ", ";
  sstr << quotes << "Status" << quotes << " : " << quotes << m_status << quotes << ", ";
  sstr << quotes << "Workgroups" << quotes << " : " << quotes << m_ncompleted << "/" << m_nworkgroups << " completed" << quotes << ", ";
  sstr << quotes << "Args" << quotes << " : " << quotes << m_args << quotes;
  return sstr.str();
}

/*
 * This is ugly but was needed so that we don't exponse
 * the appdebug::event_debug_view_base type to the xocl:: code.
 * The lambdas called from trigger_debug_action will set this
 * pointer (instead of returning, which would require exposing type to xocl)
 * The caller will read this global after returning from trigger_debug_action
 * This works because this is all part of appdebug and there is a
 * single thread of execution.
 */
static event_debug_view_base* global_return_edv;

void cb_action_readwrite (xocl::event* event, cl_mem buffer,size_t offset, size_t size, const void* ptr) {
    event_debug_view_readwrite *edv =  new event_debug_view_readwrite (
      (cl_event)event,
      event->get_uid(),
      event->get_command_type(),
      event_commandtype_to_string (event->get_command_type()),
      event_commandstatus_to_string (event->try_get_status()),
      event_dependencies_to_string(event_chain_to_dependencies (event)),
      buffer,
      offset,
      size,
      ptr
    );
    global_return_edv = edv;
}

void
cb_action_copybuf(xocl::event* event, cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset, size_t size)
{
    event_debug_view_copy *edv =  new event_debug_view_copy (
      (cl_event)event,
      event->get_uid(),
      event->get_command_type(),
      event_commandtype_to_string (event->get_command_type()),
      event_commandstatus_to_string (event->try_get_status()),
      event_dependencies_to_string(event_chain_to_dependencies (event)),
      src_buffer,
      src_offset,
      dst_buffer,
      dst_offset,
      size
    );
    global_return_edv = edv;
}

void
cb_action_fill_buffer (xocl::event* event, cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size)
{
    event_debug_view_fill *edv =  new event_debug_view_fill (
      (cl_event)event,
      event->get_uid(),
      event->get_command_type(),
      event_commandtype_to_string (event->get_command_type()),
      event_commandstatus_to_string (event->try_get_status()),
      event_dependencies_to_string(event_chain_to_dependencies (event)),
      buffer,
      offset,
      pattern,
      pattern_size,
      size
    );
    global_return_edv = edv;
}

void
cb_action_map (xocl::event* event, cl_mem buffer,cl_map_flags map_flag)
{
    event_debug_view_map *edv =  new event_debug_view_map (
      (cl_event)event,
      event->get_uid(),
      event->get_command_type(),
      event_commandtype_to_string (event->get_command_type()),
      event_commandstatus_to_string (event->try_get_status()),
      event_dependencies_to_string(event_chain_to_dependencies (event)),
      buffer,
      map_flag
    );
    global_return_edv = edv;
}

void
cb_action_migrate (xocl::event* event, cl_uint num_mem_objects, const cl_mem *mem_objects, cl_mem_migration_flags flags)
{
    event_debug_view_migrate *edv =  new event_debug_view_migrate (
      (cl_event)event,
      event->get_uid(),
      event->get_command_type(),
      event_commandtype_to_string (event->get_command_type()),
      event_commandstatus_to_string (event->try_get_status()),
      event_dependencies_to_string(event_chain_to_dependencies (event)),
      mem_objects,
      num_mem_objects,
      flags
    );
    global_return_edv = edv;
}

void
cb_action_ndrange_migrate (xocl::event* event, cl_kernel kernel)
{
    std::string kname  = xocl::xocl(kernel)->get_name();

    event_debug_view_migrate *edv =  new event_debug_view_migrate (
      (cl_event)event,
      event->get_uid(),
      event->get_command_type(),
      event_commandtype_to_string (event->get_command_type()),
      event_commandstatus_to_string (event->try_get_status()),
      event_dependencies_to_string(event_chain_to_dependencies (event)),
      kname
    );
    global_return_edv = edv;
}

void
cb_action_ndrange (xocl::event* event, cl_kernel kernel)
{
    //get cuname and workgroup information
    size_t nworkgroups = 0;
    bool issubmitted = false;
    cl_int evstatus = event->try_get_status();

    std::string kname  = xocl::xocl(kernel)->get_name();

    if (evstatus == CL_SUBMITTED || evstatus == CL_RUNNING) {
      auto exctx = event->get_execution_context();
      nworkgroups = exctx->get_num_work_groups();
      issubmitted = true;
    }
    uint32_t ncomplete = app_debug_track<cl_event>::getInstance()->try_get_data(event).m_ncomplete;
    bool has_started = app_debug_track<cl_event>::getInstance()->try_get_data(event).m_start;
    if (evstatus == CL_COMPLETE) {
      //There could be completed lingering events in the system, so need to handle completed events
      nworkgroups = ncomplete;
      issubmitted = true;
    }
    event_debug_view_ndrange *edv =  new event_debug_view_ndrange (
      (cl_event)event,
      event->get_uid(),
      event->get_command_type(),
      event_commandtype_to_string (event->get_command_type()),
      (evstatus == CL_COMPLETE) ? "Complete" : (has_started ? "Scheduled" : "Waiting"),
      event_dependencies_to_string(event_chain_to_dependencies (event)),
      kname,
      nworkgroups,
      ncomplete,
      issubmitted
    );
    global_return_edv = edv;
}

void cb_action_unmap (xocl::event* event, cl_mem buffer)
{
    event_debug_view_unmap *edv =  new event_debug_view_unmap (
      (cl_event)event,
      event->get_uid(),
      event->get_command_type(),
      event_commandtype_to_string (event->get_command_type()),
      event_commandstatus_to_string (event->try_get_status()),
      event_dependencies_to_string(event_chain_to_dependencies (event)),
      buffer
    );
    global_return_edv = edv;
}

void
cb_action_barrier_marker(xocl::event* event)
{
    event_debug_view_barrier_marker *edv =  new event_debug_view_barrier_marker (
      (cl_event)event,
      event->get_uid(),
      event->get_command_type(),
      event_commandtype_to_string (event->get_command_type()),
      event_commandstatus_to_string (event->try_get_status()),
      event_dependencies_to_string(event_chain_to_dependencies (event))
    );
    global_return_edv = edv;
}

void
cb_action_readwrite_image (xocl::event* event, cl_mem image,const size_t* origin,const size_t* region, size_t row_pitch,size_t slice_pitch,const void* ptr)
{
    event_debug_view_readwrite_image *edv = new event_debug_view_readwrite_image (
      (cl_event)event,
      event->get_uid(),
      event->get_command_type(),
      event_commandtype_to_string (event->get_command_type()),
      event_commandstatus_to_string (event->try_get_status()),
      event_dependencies_to_string(event_chain_to_dependencies (event)),
      image,
      std::vector<size_t>(origin, origin+3),
      std::vector<size_t>(region, region+3),
      row_pitch,
      slice_pitch,
      ptr
    );
    global_return_edv = edv;
}

void register_xocl_appdebug_callbacks() {
  /* event */
  xocl::event::register_constructor_callbacks(appdebug::add_event);
  xocl::event::register_destructor_callbacks(appdebug::remove_event);
  /* command queue */
  xocl::command_queue::register_constructor_callbacks(appdebug::add_command_queue);
  xocl::command_queue::register_destructor_callbacks(appdebug::remove_command_queue);

  /*cl_mem*/
  xocl::memory::register_constructor_callbacks(appdebug::add_clmem);
  xocl::memory::register_destructor_callbacks(appdebug::remove_clmem);

  /*opencl api*/
  xocl::appdebug::register_cb_action_readwrite (cb_action_readwrite);
  xocl::appdebug::register_cb_action_copybuf (cb_action_copybuf);
  xocl::appdebug::register_cb_action_fill_buffer (cb_action_fill_buffer);
  xocl::appdebug::register_cb_action_map (cb_action_map);
  xocl::appdebug::register_cb_action_migrate (cb_action_migrate);
  xocl::appdebug::register_cb_action_ndrange_migrate (cb_action_ndrange_migrate);
  xocl::appdebug::register_cb_action_ndrange (cb_action_ndrange);
  xocl::appdebug::register_cb_action_unmap (cb_action_unmap);
  xocl::appdebug::register_cb_action_barrier_marker (cb_action_barrier_marker);
  xocl::appdebug::register_cb_action_readwrite_image (cb_action_readwrite_image);
}

inline
void try_get_queue_sizes (cl_command_queue cq, size_t& nQueued, size_t& nSubmitted) {
  //Assumes that cq is validated

  //The lambda cannot call any functions that will lock tracker, as the lock
  //is already claimed by for_each
  auto fLambdaCounter = [cq, &nQueued, &nSubmitted] (cl_event aEvent) {
    if (xocl::xocl(aEvent)->get_command_queue() == cq) {
      if (xocl::xocl(aEvent)->try_get_status() == CL_QUEUED)
        ++nQueued;
      else
        ++nSubmitted;
    }
  };
  //Iterate all events that are in cq and count based on their status (queued/submitted)
  nQueued = 0; nSubmitted = 0;
  app_debug_track<cl_event>::getInstance()->for_each(std::move(fLambdaCounter));
}

//Debug functions called from GDB
app_debug_view<std::pair<size_t,size_t>>*
clPrintCmdQOccupancy(cl_command_queue cq)
{
  std::pair<size_t, size_t> *size_pair = new std::pair<size_t,size_t> (SIZE_MAX, SIZE_MAX);
  auto adv = new app_debug_view<std::pair<size_t,size_t>> (size_pair, [size_pair](){delete size_pair;});
  try {
    validate_command_queue(cq);
  }
  catch (const xocl::error &ex) {
    adv->setInvalidMsg(true, ex.what());
    return adv;
  }

  try {
    try_get_queue_sizes(cq, size_pair->first, size_pair->second);
  }
  catch (const xocl::error & ex) {
    adv->setInvalidMsg(true, ex.what());
  }
  return adv;
}

app_debug_view <std::vector<event_debug_view_base*> >*
clPrintCmdQQueued(cl_command_queue cq)
{
  size_t nq, ns;
  try {
    validate_command_queue(cq);
  }
  catch (const xocl::error &ex) {
    auto adv = new app_debug_view <std::vector<event_debug_view_base*> > (nullptr, nullptr, true, ex.what());
    return adv;
  }

  try {
    try_get_queue_sizes(cq, nq, ns);
  }
  catch (const xocl::error& ex) {
    auto adv = new app_debug_view <std::vector<event_debug_view_base*> > (nullptr, nullptr, true, ex.what());
    return adv;
  }

  std::vector<event_debug_view_base*> *v = new std::vector<event_debug_view_base*>();

  std::vector<xocl::event*> selectedEventsVec;

  auto collect_events_lamda = [cq, &selectedEventsVec] (cl_event aEvent) {
    xocl::event *e = xocl::xocl(aEvent);
    if (e->get_command_queue() == cq && e->try_get_status() == CL_QUEUED)
      selectedEventsVec.push_back(e);
  };

  auto add_edv_lambda = [v](xocl::event *e) {
    v->push_back((e->trigger_debug_action(), global_return_edv));
  };

  auto delete_edv_lambda = [v]() {
    for (auto edv : *v)
      delete edv;
    delete v;
  };
  auto adv = new app_debug_view <std::vector<event_debug_view_base*> > (v, std::move(delete_edv_lambda));

  try {
    //First collect the events of interest in a vector and then call debug actions on them
    //for_each and debug action both need the lock on the tracker resulting in deadlock.
    app_debug_track<cl_event>::getInstance()->for_each(std::move(collect_events_lamda));

    std::for_each(selectedEventsVec.begin(), selectedEventsVec.end(), std::move(add_edv_lambda));
  }
  catch(const xocl::error& ex) {
    adv->setInvalidMsg(true, ex.what());
  }
  return adv;
}

app_debug_view <std::vector<event_debug_view_base*> >*
clPrintCmdQSubmitted(cl_command_queue cq)
{
  size_t nq, ns;
  try {
    validate_command_queue(cq);
  }
  catch (const xocl::error& ex) {
    auto adv = new app_debug_view <std::vector<event_debug_view_base*> > (nullptr, nullptr, true, ex.what());
    return adv;
  }

  try {
    try_get_queue_sizes(cq, nq, ns);
  }
  catch (const xocl::error& ex) {
    auto adv = new app_debug_view <std::vector<event_debug_view_base*> > (nullptr, nullptr, true, ex.what());
    return adv;
  }
  std::vector<event_debug_view_base*> *v = new std::vector<event_debug_view_base*>();
  std::vector<xocl::event*> selectedEventsVec;

  auto collect_events_lamda = [cq, &selectedEventsVec] (cl_event aEvent) {
    xocl::event* e = xocl::xocl(aEvent);
    if (e->get_command_queue() == cq && e->try_get_status() != CL_QUEUED)
      selectedEventsVec.push_back(e);
  };

  auto add_edv_lambda = [v](xocl::event *e) {
    v->push_back((e->trigger_debug_action(), global_return_edv));
  };

  auto delete_edv_lambda = [v]() {
    for (auto edv : *v)
      delete edv;
    delete v;
  };
  auto adv = new app_debug_view <std::vector<event_debug_view_base*> > (v, std::move(delete_edv_lambda));

  try {
    //First collect the events of interest in a vector and then call debug actions on them
    //for_each and debug action both need the lock on the tracker resulting in deadlock.
    app_debug_track<cl_event>::getInstance()->for_each(std::move(collect_events_lamda));
    std::for_each(selectedEventsVec.begin(), selectedEventsVec.end(), std::move(add_edv_lambda));
  }
  catch(const xocl::error& ex) {
    adv->setInvalidMsg(true, ex.what());
  }
  return adv;
}
void
clFreeAppDebugView(app_debug_view_base* aView)
{
  if (!aView) return;
  delete aView;
}

app_debug_view <clmem_debug_view>*
clGetMemInfo(cl_mem aMem) {
  uint64_t addr;
  std::string bank;
  clmem_debug_view *mdv;

  try {
    validate_clmem(aMem);
  }
  catch (const xocl::error& ex) {
    auto adv = new app_debug_view <clmem_debug_view> (nullptr, nullptr, true, ex.what());
    return adv;
  }

  auto xoclMem = xocl::xocl(aMem);
  try {
    xoclMem->try_get_address_bank(addr, bank);
    mdv = new clmem_debug_view(aMem, xoclMem->get_uid(), bank, addr,
                             xoclMem->get_size(), xoclMem->get_host_ptr());
  }
  catch (const xocl::error& ex) {
    //locked, cannot provide addr/bank information

    mdv = new clmem_debug_view(aMem, xoclMem->get_uid(), "Unknown", std::numeric_limits<uint64_t>::max(),
                             xoclMem->get_size(), xoclMem->get_host_ptr());

    auto adv = new app_debug_view <clmem_debug_view> (mdv, [mdv] () {delete mdv;}, true, ex.what());
    return adv;
  }
  auto adv = new app_debug_view <clmem_debug_view> (mdv, [mdv] () {delete mdv;}, false, "");
  return adv;
}

app_debug_view<event_debug_view_base>*
clGetEventInfo(cl_event aEvent) {
  event_debug_view_base* edv;
  //aEvent could be an event that doesn't have trigger_debug_action set
  //When trigger_debug_action the exception is caught check if app_
  try {
    validate_event(aEvent);
  }
  catch (const xocl::error &ex) {
      auto adv = new app_debug_view <event_debug_view_base> (nullptr, nullptr, true, ex.what());
      return adv;
  }
  auto xoclEvent = xocl::xocl(aEvent);
  try {
    edv = (xoclEvent->trigger_debug_action(), global_return_edv);
  }
  catch (const xocl::error & ex) {
    if (ex.get() == DBG_EXCEPT_NO_DBG_ACTION) {
      //if no debug action set then return basic information
      edv =  new event_debug_view_base (aEvent,
                                        xoclEvent->get_uid(),
                                        xoclEvent->get_command_type(),
                                        event_commandtype_to_string (xoclEvent->get_command_type()),
                                        event_commandstatus_to_string (xoclEvent->try_get_status()),
                                        event_dependencies_to_string(event_chain_to_dependencies (xoclEvent)));
    }
    else {
      //some other exception
      auto adv = new app_debug_view <event_debug_view_base> (nullptr, nullptr, true, ex.what());
      return adv;
    }
  }
  auto adv = new app_debug_view <event_debug_view_base> (edv, [edv] () {delete edv;}, false, "");
  return adv;
}

app_debug_view<std::vector<cl_command_queue>>*
clGetCmdQueues()
{
  std::vector<cl_command_queue> *v = new std::vector<cl_command_queue>();
  auto adv = new app_debug_view<std::vector<cl_command_queue>> (v, [v](){delete v;});
  try {
    app_debug_track<cl_command_queue>::getInstance()->for_each([v](cl_command_queue aQueue) {v->push_back(aQueue);});
  }
  catch (const xocl::error &ex) {
    adv->setInvalidMsg(true, ex.what());
    return adv;
  }
  return adv;
}

app_debug_view<std::vector<cl_mem>>*
clGetClMems()
{
  std::vector<cl_mem> *v = new std::vector<cl_mem>();
  auto adv = new app_debug_view<std::vector<cl_mem>> (v, [v](){delete v;});
  try {
    app_debug_track<cl_mem>::getInstance()->for_each([v](cl_mem aMem) {v->push_back(aMem);});
  }
  catch (const xocl::error &ex) {
    adv->setInvalidMsg(true, ex.what());
    return adv;
  }

  return adv;
}

static std::string
getScalarArgValue(const xocl::kernel* kernel, const xocl::kernel::xargument* arg)
{
  std::stringstream sstr;
  std::vector<uint32_t> value = xrt_core::kernel_int::get_arg_value(kernel->get_xrt_run(nullptr), arg->get_argidx());
  const uint8_t* data = reinterpret_cast<uint8_t*>(value.data());
  auto bytes = value.size() * sizeof(uint32_t);
  auto hostsize = arg->get_hostsize();
  if (bytes < hostsize)
    return "bad scalar argument value";

  auto hosttype = arg->get_hosttype();
  if (hosttype == "float" || hosttype == "double") {
    if (hostsize == 64)
      sstr << *(reinterpret_cast<const double*>(data));
    else
      sstr << *(reinterpret_cast<const float*>(data));
  }
  else {
    sstr << "0x";
    for (int i = hostsize - 1; i >= 0; --i)
      sstr << std::hex << std::setw(2) << std::setfill('0') << data[i];
  }

  return sstr.str();
}

static std::string
getGlobalArgValue(const xocl::kernel* kernel, const xocl::kernel::xargument* arg)
{
  std::stringstream sstr;
  if (auto mem = arg->get_memory_object()) {
    uint64_t physaddr = 0;
    std::string bank = "";
    xocl::xocl(mem)->try_get_address_bank(physaddr, bank);
    sstr << "0x" << std::hex << physaddr << std::dec << "(Bank-" << bank << ")";
  }
  return sstr.str();
}

static std::string
getArgValueString(const xocl::event* aEvent)
{
  std::stringstream sstr;
  auto ctx = aEvent->get_execution_context();
  auto kernel = ctx->get_kernel();
  for (auto& arg : kernel->get_indexed_xargument_range()) {
    sstr << arg->get_name() << " = ";
    switch (arg->get_argtype()) {
    case xrt_core::xclbin::kernel_argument::argtype::scalar:
      sstr << getScalarArgValue(kernel, arg.get());
      break;
    case xrt_core::xclbin::kernel_argument::argtype::global:
    case xrt_core::xclbin::kernel_argument::argtype::constant:
      sstr << getGlobalArgValue(kernel, arg.get());
      break;
    case xrt_core::xclbin::kernel_argument::argtype::stream:
      sstr << "stream arg";
      break;
    case xrt_core::xclbin::kernel_argument::argtype::local:
      sstr << "local arg";
      break;
    }
    sstr << " ";
  }

  return sstr.str();
}

app_debug_view<std::vector<kernel_debug_view*> >*
clGetKernelInfo()
{
  std::vector<kernel_debug_view*> *v = new std::vector<kernel_debug_view*> ();
  auto delete_kdv_lamda = [v] {
    for (auto kdv : *v)
      delete kdv;
    delete v;
  };
  auto adv = new app_debug_view <std::vector<kernel_debug_view*> > (v, std::move(delete_kdv_lamda), false, "");

  std::vector<xocl::event*> selectedEventsVec;

  auto collect_kernel_events_lamda = [&selectedEventsVec] (cl_event aEvent) {
    xocl::event* e = xocl::xocl(aEvent);
    if (e->get_command_type() == CL_COMMAND_NDRANGE_KERNEL || e->get_command_type() == CL_COMMAND_TASK)
      selectedEventsVec.push_back(e);
  };

  auto add_edv_lambda = [v](xocl::event *event) {
    if (event!=nullptr) {
      try {
        cl_int evstatus = event->try_get_status();
        if (evstatus == CL_SUBMITTED || evstatus == CL_RUNNING) {
          auto exctx = event->get_execution_context();
          std::string kname = exctx->get_kernel()->get_name();

          bool is_scheduled = app_debug_track<cl_event>::getInstance()->try_get_data(event).m_start;
          uint32_t ncomplete = app_debug_track<cl_event>::getInstance()->try_get_data(event).m_ncomplete;
          std::string evstatusstr = is_scheduled ? "Scheduled" : "Waiting";
          v->push_back(new kernel_debug_view (kname, evstatusstr, exctx->get_num_work_groups(), ncomplete, getArgValueString(event)));
        }
      }
      catch (const xocl::error &ex) {
        v->push_back(new kernel_debug_view ("None", "None", 0, 0, ex.what()));
      }
    }
  };

  try {
    //First collect the events of interest in a vector and then call debug actions on them
    //for_each and debug action both need the lock on the tracker resulting in deadlock.
    app_debug_track<cl_event>::getInstance()->for_each(std::move(collect_kernel_events_lamda));
    std::for_each(selectedEventsVec.begin(), selectedEventsVec.end(), std::move(add_edv_lambda));
  }
  catch(const xocl::error& ex) {
    adv->setInvalidMsg(true, ex.what());
  }
  return adv;
}
bool
isAppdebugEnabled()
{
  return xrt_xocl::config::get_app_debug();
}

uint32_t getIPCountAddrNames(std::string& devUserName, int type, std::vector<uint64_t> *baseAddress, std::vector<std::string> * portNames) {
  debug_ip_layout *map;
  // Get the path to the device from the HAL
  // std::string path = "/sys/bus/pci/devices/" + devUserName + "/debug_ip_layout";
  std::string path = devUserName;
  std::ifstream ifs(path.c_str(), std::ifstream::binary);
  uint32_t count = 0;
  char buffer[debug_ip_layout_max_size];
  if( ifs.good() ) {
    //sysfs max file size is debug_ip_layout_max_size
    ifs.read(buffer, debug_ip_layout_max_size);
    if (ifs.gcount() > 0) {
      map = (debug_ip_layout*)(buffer);
      for( unsigned int i = 0; i < map->m_count; i++ ) {
        if (map->m_debug_ip_data[i].m_type == type) {
          if(baseAddress)baseAddress->push_back(map->m_debug_ip_data[i].m_base_address);
          if(portNames) portNames->push_back((char*)map->m_debug_ip_data[i].m_name);
          ++count;
        }
      }
    }
    ifs.close();
  }
  return count;
}

std::pair<size_t, size_t> getCUNamePortName (std::vector<std::string>& aSlotNames,
                                             std::vector< std::pair<std::string, std::string> >& aCUNamePortNames) {
    //Slotnames are of the format "/cuname/portname" or "cuname/portname", split them and return in separate vector
    //return max length of the cuname and port names
    size_t max1 = 0, max2 = 0;
    char sep = '/';
    for (const auto& slotName: aSlotNames) {
        size_t found1;
        size_t start = 0;
        found1 = slotName.find(sep, 0);
        if (found1 == 0) {
            //if the cuname starts with a '/'
            start = 1;
            found1 = slotName.find(sep, 1);
        }
        if (found1 != std::string::npos) {
            aCUNamePortNames.emplace_back(slotName.substr(start, found1-start), slotName.substr(found1+1));
        }
        else {
            aCUNamePortNames.emplace_back("Unknown", "Unknown");
        }
        //Replace the name of the host-aim to something simple
        if (aCUNamePortNames.back().first.find("interconnect_host_aximm") != std::string::npos) {
            aCUNamePortNames.pop_back();
            aCUNamePortNames.emplace_back("XDMA", "N/A");
        }

        // Use strlen() instead of length() because the strings taken from debug_ip_layout
        // are always 128 in length, where the end is full of null characters
        max1 = std::max(strlen(aCUNamePortNames.back().first.c_str()), max1);
        max2 = std::max(strlen(aCUNamePortNames.back().second.c_str()), max2);
    }
    return std::pair<size_t, size_t>(max1, max2);
}

struct aim_debug_view {
  unsigned long long int WriteBytes   [xdp::MAX_NUM_AIMS];
  unsigned long long int WriteTranx   [xdp::MAX_NUM_AIMS];
  unsigned long long int ReadBytes    [xdp::MAX_NUM_AIMS];
  unsigned long long int ReadTranx    [xdp::MAX_NUM_AIMS];

  unsigned long long int OutStandCnts [xdp::MAX_NUM_AIMS];
  unsigned long long int LastWriteAddr[xdp::MAX_NUM_AIMS];
  unsigned long long int LastWriteData[xdp::MAX_NUM_AIMS];
  unsigned long long int LastReadAddr [xdp::MAX_NUM_AIMS];
  unsigned long long int LastReadData [xdp::MAX_NUM_AIMS];
  unsigned int   NumSlots;
  std::string    DevUserName;
  std::string    SysfsPath;
  aim_debug_view () {
    std::fill(WriteBytes,    WriteBytes    + xdp::MAX_NUM_AIMS, 0);
    std::fill(WriteTranx,    WriteTranx    + xdp::MAX_NUM_AIMS, 0);
    std::fill(ReadBytes,     ReadBytes     + xdp::MAX_NUM_AIMS, 0);
    std::fill(ReadTranx,     ReadTranx     + xdp::MAX_NUM_AIMS, 0);
    std::fill(OutStandCnts,  OutStandCnts  + xdp::MAX_NUM_AIMS, 0);
    std::fill(LastWriteAddr, LastWriteAddr + xdp::MAX_NUM_AIMS, 0);
    std::fill(LastWriteData, LastWriteData + xdp::MAX_NUM_AIMS, 0);
    std::fill(LastReadAddr,  LastReadAddr  + xdp::MAX_NUM_AIMS, 0);
    std::fill(LastReadData,  LastReadData  + xdp::MAX_NUM_AIMS, 0);
    NumSlots = 0;
    DevUserName = "";
  }
  ~aim_debug_view() {}
  std::string getstring(int aVerbose = 0, int aJSONFormat = 0);
};

std::string
aim_debug_view::getstring(int aVerbose, int aJSONFormat) {
  std::stringstream sstr;
  std::vector<std::string> slotNames;
  std::vector< std::pair<std::string, std::string> > cuNameportNames;

  std::string quotes;
  if (aJSONFormat) quotes = "\"";
  else  quotes = "";
  if (NumSlots == 0) {
    if (aJSONFormat) {
      return "[]";
    }
    else {
      sstr << "No AXI Interface Monitors (AIM) found on the platform \n";
      return sstr.str();
    }
  }

  //  unsigned int numSlots =
  getIPCountAddrNames(SysfsPath, AXI_MM_MONITOR, nullptr, &slotNames);
  std::pair<size_t, size_t> widths = getCUNamePortName(slotNames, cuNameportNames);

  if (aJSONFormat) {
    sstr << "["; //spm list
      for (unsigned int i = 0; i<NumSlots; ++i) {
         sstr << (i > 0 ? "," : "") << "{";
         sstr << quotes << "RegionCU" << quotes << " : " << quotes << cuNameportNames[i].first << quotes << ",";
         sstr << quotes << "TypePort" << quotes << " : " << quotes << cuNameportNames[i].second.c_str() << quotes << ",";
         sstr << quotes << "WriteBytes" << quotes << " : " << quotes <<  WriteBytes[i] << quotes << ",";
         sstr << quotes << "WriteTranx" << quotes << " : " << quotes <<  WriteTranx[i] << quotes << ",";
         sstr << quotes << "ReadBytes" << quotes << " : " << quotes <<  ReadBytes[i] << quotes << ",";
         sstr << quotes << "ReadTranx" << quotes << " : " << quotes <<  ReadTranx[i] << quotes << ",";
         sstr << quotes << "OutstandingCnt" << quotes << " : " << quotes <<  OutStandCnts[i] << quotes << ",";
         sstr << quotes << "LastWrAddr" << quotes << " : " << quotes << "0x" << std::hex << LastWriteAddr[i] << quotes << ",";
         sstr << quotes << "LastWrData" << quotes << " : " << quotes << "0x" << LastWriteData[i] << quotes << ",";
         sstr << quotes << "LastRdAddr" << quotes << " : " << quotes << "0x" << LastReadAddr[i]  << quotes << ",";
         sstr << quotes << "LastRdData" << quotes << " : " << quotes << "0x" << LastReadData[i]  << quotes << std::dec ;
         sstr << "}";
      }
    sstr << "]";
  }
  else {
    sstr<< "AXI Interface Monitor (AIM) Counters\n";
    auto col1 = std::max(widths.first, strlen("Region or CU")) + 4;
    auto col2 = std::max(widths.second, strlen("Type or Port"));

    sstr << std::left
              << std::setw(col1) << "Region or CU"
              << " " << std::setw(col2) << "Type or Port"
              << "  " << std::setw(16)  << "Write Bytes"
              << "  " << std::setw(16)  << "Write Tranx."
              << "  " << std::setw(16)  << "Read Bytes"
              << "  " << std::setw(16)  << "Read Tranx."
              << "  " << std::setw(16)  << "Outstanding Cnt"
              << "  " << std::setw(16)  << "Last Wr Addr"
              << "  " << std::setw(16)  << "Last Wr Data"
              << "  " << std::setw(16)  << "Last Rd Addr"
              << "  " << std::setw(16)  << "Last Rd Data"
              << std::endl;
    for (unsigned int i = 0; i<NumSlots; ++i) {
      sstr << std::left
              << std::setw(col1) << cuNameportNames[i].first
              << " " << std::setw(col2) << cuNameportNames[i].second.c_str()
              << "  " << std::setw(16) << WriteBytes[i]
              << "  " << std::setw(16) << WriteTranx[i]
              << "  " << std::setw(16) << ReadBytes[i]
              << "  " << std::setw(16) << ReadTranx[i]
              << "  " << std::setw(16) << OutStandCnts[i]
              << std::hex
              << "  " << "0x" << std::setw(14) << LastWriteAddr[i]
              << "  " << "0x" << std::setw(14) << LastWriteData[i]
              << "  " << "0x" << std::setw(14) << LastReadAddr[i]
              << "  " << "0x" << std::setw(14) << LastReadData[i]
              << std::dec << std::endl;
    }
  }
  return sstr.str();
}

static bool
isEmulationMode()
{
  static bool val = (std::getenv("XCL_EMULATION_MODE") != nullptr);
  return val;
}

app_debug_view<aim_debug_view>*
clGetDebugCounters() {
  xdp::AIMCounterResults debugResults = {0};

  if (isEmulationMode()) {
    auto adv = new app_debug_view<aim_debug_view>(nullptr, nullptr, true, "xstatus is not supported in emulation flow");
    return adv;
  }

  if (!appdebug::active()) {
    auto adv = new app_debug_view<aim_debug_view>(nullptr, nullptr, true, "Runtime instance not yet created");
    return adv;
  }

  auto platform = appdebug::getcl_platform_id();
  // Iterates over all devices, but assumes only one device
  memset(&debugResults,0, sizeof(xdp::AIMCounterResults));
  std::string subdev = "icap";
  std::string entry = "debug_ip_layout";
  std::string sysfs_open_path;
  for (auto device : platform->get_device_range()) {
    if (device->is_active()) {
      //memset(&debugResults,0, sizeof(xdp::AIMCounterResults));
      //At this point we deal with only one deviceyy
      device->get_xdevice()->debugReadIPStatus(XCL_DEBUG_READ_TYPE_AIM, &debugResults);
      sysfs_open_path = device->get_xdevice()->getSysfsPath(subdev, entry).get();
      //ret |= xdp::profile::device::debugReadIPStatus(device, XCL_DEBUG_READ_TYPE_AIM, &debugResults);
    }
  }

  auto aim_view = new aim_debug_view ();
  std::copy(debugResults.WriteBytes, debugResults.WriteBytes+xdp::MAX_NUM_AIMS, aim_view->WriteBytes);
  std::copy(debugResults.WriteTranx, debugResults.WriteTranx+xdp::MAX_NUM_AIMS, aim_view->WriteTranx);
  std::copy(debugResults.ReadBytes, debugResults.ReadBytes+xdp::MAX_NUM_AIMS, aim_view->ReadBytes);
  std::copy(debugResults.ReadTranx, debugResults.ReadTranx+xdp::MAX_NUM_AIMS, aim_view->ReadTranx);
  std::copy(debugResults.OutStandCnts, debugResults.OutStandCnts+xdp::MAX_NUM_AIMS, aim_view->OutStandCnts);
  std::copy(debugResults.LastWriteAddr, debugResults.LastWriteAddr+xdp::MAX_NUM_AIMS, aim_view->LastWriteAddr);
  std::copy(debugResults.LastWriteData, debugResults.LastWriteData+xdp::MAX_NUM_AIMS, aim_view->LastWriteData);
  std::copy(debugResults.LastReadAddr, debugResults.LastReadAddr+xdp::MAX_NUM_AIMS, aim_view->LastReadAddr);
  std::copy(debugResults.LastReadData, debugResults.LastReadData+xdp::MAX_NUM_AIMS, aim_view->LastReadData);
  aim_view->NumSlots = debugResults.NumSlots;
  aim_view->DevUserName = debugResults.DevUserName;
  aim_view->SysfsPath = sysfs_open_path;

  auto adv = new app_debug_view <aim_debug_view> (aim_view, [aim_view](){delete aim_view;}, false, "");

  return adv;
}

// Streaming counter view

struct asm_debug_view {
  unsigned long long int StrNumTranx    [xdp::MAX_NUM_ASMS];
  unsigned long long int StrDataBytes   [xdp::MAX_NUM_ASMS];
  unsigned long long int StrBusyCycles  [xdp::MAX_NUM_ASMS];
  unsigned long long int StrStallCycles [xdp::MAX_NUM_ASMS];
  unsigned long long int StrStarveCycles[xdp::MAX_NUM_ASMS];

  unsigned int NumSlots ;
  std::vector<std::pair<std::string, std::string> > ConnectionNames;
  std::string  DevUserName ;
  std::string    SysfsPath;

  asm_debug_view()
  {
    std::fill(StrNumTranx,     StrNumTranx     + xdp::MAX_NUM_ASMS, 0);
    std::fill(StrDataBytes,    StrDataBytes    + xdp::MAX_NUM_ASMS, 0);
    std::fill(StrBusyCycles,   StrBusyCycles   + xdp::MAX_NUM_ASMS, 0);
    std::fill(StrStallCycles,  StrStallCycles  + xdp::MAX_NUM_ASMS, 0);
    std::fill(StrStarveCycles, StrStarveCycles + xdp::MAX_NUM_ASMS, 0);

    NumSlots = 0;
  }
  ~asm_debug_view() { }
  std::string getstring(int aVerbose = 0, int aJSONFormat = 0);

  std::string getJSONString(bool aVerbose) ;
  std::string getXGDBString(bool aVerbose) ;
} ;

std::string
asm_debug_view::getstring(int aVerbose, int aJSONFormat) {
  if (aJSONFormat) return getJSONString(aVerbose != 0 ? true : false) ;
  else return getXGDBString(aVerbose != 0 ? true : false) ;
}

std::string
asm_debug_view::getJSONString(bool aVerbose) {
  std::stringstream sstr ;

  sstr << "[" ;
  for (unsigned int i = 0 ; i < NumSlots ; ++i)
  {
    if (i > 0) sstr << "," ;
    sstr << "{" ;
    sstr << "\"" << "StrNumTransactions"  << "\"" << ":"
	 << "\"" << StrNumTranx[i] << "\"" << "," ;
    sstr << "\"" << "StrDataBytes"  << "\"" << ":"
	 << "\"" << StrDataBytes[i] << "\"" << "," ;
    sstr << "\"" << "StrBusyCycles"  << "\"" << ":"
	 << "\"" << StrBusyCycles[i] << "\"" << "," ;
    sstr << "\"" << "StrStallCycles"  << "\"" << ":"
	 << "\"" << StrStallCycles[i] << "\"" << "," ;
    sstr << "\"" << "StrStarveCycles"  << "\"" << ":"
	 << "\"" << StrStarveCycles[i] << "\"" ;
    sstr << "}" ;
  }
  sstr << "]" ;

  return sstr.str();
}

std::string
asm_debug_view::getXGDBString(bool aVerbose) {
  std::stringstream sstr;

  // Calculate the width for formatting the columns
  size_t maxMasterWidth = 0 ;
  size_t maxSlaveWidth  = 0 ;
  for (unsigned int i = 0 ; i < NumSlots ; ++i)
  {
    maxMasterWidth = std::max(ConnectionNames[i].first.length(), maxMasterWidth);
    maxSlaveWidth = std::max(ConnectionNames[i].second.length(), maxSlaveWidth);
  }

  auto col1 = std::max(strlen("Stream Master"), maxMasterWidth) + 4 ;
  auto col2 = std::max(strlen("Stream Slave"), maxSlaveWidth) ;

  sstr << "AXI Stream Monitor (ASM) Counters\n" ;
  sstr << std::left
               << std::setw(col1) << "Stream Master"
       << "  " << std::setw(col2) << "Stream Slave"
       << "  " << std::setw(32)   << "Number of Transactions"
       << "  " << std::setw(16)   << "Data Bytes"
       << "  " << std::setw(16)   << "Busy Cycles"
       << "  " << std::setw(16)   << "Stall Cycles"
       << "  " << std::setw(16)   << "Starve Cycles"
       << std::endl ;
  for (unsigned int i = 0 ; i < NumSlots ; ++i)
  {
    sstr << std::left
	         << std::setw(col1) << ConnectionNames[i].first
	 << "  " << std::setw(col2) << ConnectionNames[i].second
	 << "  " << std::setw(32)   << StrNumTranx[i]
	 << "  " << std::setw(16)   << StrDataBytes[i]
	 << "  " << std::setw(16)   << StrBusyCycles[i]
	 << "  " << std::setw(16)   << StrStallCycles[i]
	 << "  " << std::setw(16)   << StrStarveCycles[i]
	 << std::endl ;
  }

  return sstr.str() ;
}

app_debug_view<asm_debug_view>*
clGetDebugStreamCounters()
{
  // Check for error conditions where we cannot read the streaming counters
  if (isEmulationMode()) {
    auto adv = new app_debug_view<asm_debug_view>(nullptr, nullptr, true, "xstatus is not supported in emulation flow");
    return adv;
  }
  if (!appdebug::active()) {
    auto adv = new app_debug_view<asm_debug_view>(nullptr, nullptr, true, "Runtime instance not yet created");
    return adv;
  }

  xdp::ASMCounterResults streamingDebugCounters;
  memset(&streamingDebugCounters, 0, sizeof(xdp::ASMCounterResults));
  std::string subdev = "icap";
  std::string entry = "debug_ip_layout";
  std::string sysfs_open_path;
  auto platform = appdebug::getcl_platform_id();
  for (auto device : platform->get_device_range())
  {
    if (device->is_active())
    {
      // At this point, we are dealing with only one device
      device->get_xdevice()->debugReadIPStatus(XCL_DEBUG_READ_TYPE_ASM, &streamingDebugCounters);
      sysfs_open_path = device->get_xdevice()->getSysfsPath(subdev, entry).get();
      //ret |= xdp::profile::device::debugReadIPStatus(device, XCL_DEBUG_READ_TYPE_ASM, &streamingDebugCounters);
    }
  }

  auto asm_view = new asm_debug_view () ;

  std::copy(streamingDebugCounters.StrNumTranx,
	    streamingDebugCounters.StrNumTranx+xdp::MAX_NUM_ASMS,
	    asm_view->StrNumTranx);
  std::copy(streamingDebugCounters.StrDataBytes,
	    streamingDebugCounters.StrDataBytes+xdp::MAX_NUM_ASMS,
	    asm_view->StrDataBytes);
  std::copy(streamingDebugCounters.StrBusyCycles,
	    streamingDebugCounters.StrBusyCycles+xdp::MAX_NUM_ASMS,
	    asm_view->StrBusyCycles);
  std::copy(streamingDebugCounters.StrStallCycles,
	    streamingDebugCounters.StrStallCycles+xdp::MAX_NUM_ASMS,
	    asm_view->StrStallCycles);
  std::copy(streamingDebugCounters.StrStarveCycles,
	    streamingDebugCounters.StrStarveCycles+xdp::MAX_NUM_ASMS,
	    asm_view->StrStarveCycles);

  asm_view->NumSlots    = streamingDebugCounters.NumSlots ;
  asm_view->DevUserName = streamingDebugCounters.DevUserName ;
  asm_view->SysfsPath = sysfs_open_path;

  std::vector<std::string> slotNames;
  getIPCountAddrNames(sysfs_open_path, AXI_STREAM_MONITOR, nullptr, &slotNames);

  for (auto& s : slotNames)
  {
    size_t found ;
    // Stream monitor names are constructed as Master-Slave
    found = s.find("-", 0) ;
    if (found != std::string::npos)
      asm_view->ConnectionNames.push_back(std::make_pair(s.substr(0, found), s.substr(found+1)));
    else
      asm_view->ConnectionNames.push_back(std::make_pair("Unknown", "Unknown"));
  }

  auto adv = new app_debug_view<asm_debug_view>(asm_view, [asm_view]() { delete asm_view;}, false, "") ;
  return adv ;
}

// Accel monitor
struct am_debug_view {
  unsigned long long CuExecCount      [xdp::MAX_NUM_AMS];
  unsigned long long CuExecCycles     [xdp::MAX_NUM_AMS];
  unsigned long long CuBusyCycles     [xdp::MAX_NUM_AMS];
  unsigned long long CuMaxParallelIter[xdp::MAX_NUM_AMS];
  unsigned long long CuStallExtCycles [xdp::MAX_NUM_AMS];
  unsigned long long CuStallIntCycles [xdp::MAX_NUM_AMS];
  unsigned long long CuStallStrCycles [xdp::MAX_NUM_AMS];
  unsigned long long CuMinExecCycles  [xdp::MAX_NUM_AMS];
  unsigned long long CuMaxExecCycles  [xdp::MAX_NUM_AMS];
  unsigned long long CuStartCount     [xdp::MAX_NUM_AMS];

  unsigned int NumSlots ;
  std::string DevUserName ;
  std::string SysfsPath;

  am_debug_view()
  {
    std::fill(CuExecCount,       CuExecCount       + xdp::MAX_NUM_AMS, 0);
    std::fill(CuExecCycles,      CuExecCycles      + xdp::MAX_NUM_AMS, 0);
    std::fill(CuBusyCycles,      CuBusyCycles      + xdp::MAX_NUM_AMS, 0);
    std::fill(CuMaxParallelIter, CuMaxParallelIter + xdp::MAX_NUM_AMS, 0);
    std::fill(CuStallExtCycles,  CuStallExtCycles  + xdp::MAX_NUM_AMS, 0);
    std::fill(CuStallIntCycles,  CuStallIntCycles  + xdp::MAX_NUM_AMS, 0);
    std::fill(CuStallStrCycles,  CuStallStrCycles  + xdp::MAX_NUM_AMS, 0);
    std::fill(CuMinExecCycles,   CuMinExecCycles   + xdp::MAX_NUM_AMS, 0);
    std::fill(CuMaxExecCycles,   CuMaxExecCycles   + xdp::MAX_NUM_AMS, 0);
    std::fill(CuStartCount,      CuStartCount      + xdp::MAX_NUM_AMS, 0);

    NumSlots = 0;
  }
  ~am_debug_view() { }
  std::string getstring(int aVerbose = 0, int aJSONFormat = 0);

  std::string getJSONString(bool aVerbose) ;
  std::string getXGDBString(bool aVerbose) ;
} ;

std::string
am_debug_view::getstring(int aVerbose, int aJSONFormat) {
  if (aJSONFormat) return getJSONString(aVerbose != 0 ? true : false) ;
  else return getXGDBString(aVerbose != 0 ? true : false) ;
}

std::string
am_debug_view::getJSONString(bool aVerbose) {
  std::stringstream sstr ;
  std::vector<std::string> slotNames;
  getIPCountAddrNames(SysfsPath, ACCEL_MONITOR, nullptr, &slotNames);

  sstr << "[" ;
  for (unsigned int i = 0 ; i < NumSlots ; ++i)
  {
    if (i > 0) sstr << "," ;
    sstr << "{" ;
    sstr << "\"" << "CuName"  << "\"" << ":"
	 << "\"" << slotNames[i] << "\"" << "," ;
    sstr << "\"" << "CuExecCount"  << "\"" << ":"
	 << "\"" << CuExecCount[i] << "\"" << "," ;
    sstr << "\"" << "CuExecCycles"  << "\"" << ":"
	 << "\"" << CuExecCycles[i] << "\"" << "," ;
    sstr << "\"" << "CuBusyCycles"  << "\"" << ":"
	     << "\"" << CuBusyCycles[i] << "\"" << "," ;
    sstr << "\"" << "CuMaxParallelIter"  << "\"" << ":"
	     << "\"" << CuMaxParallelIter[i] << "\"" << "," ;
    sstr << "\"" << "CuStallExtCycles"  << "\"" << ":"
	 << "\"" << CuStallExtCycles[i] << "\"" << "," ;
    sstr << "\"" << "CuStallIntCycles"  << "\"" << ":"
	 << "\"" << CuStallIntCycles[i] << "\"" << "," ;
    sstr << "\"" << "CuStallStrCycles"  << "\"" << ":"
	 << "\"" << CuStallStrCycles[i] << "\"" ;
    sstr << "\"" << "CuMinExecCycles"  << "\"" << ":"
	 << "\"" << CuMinExecCycles[i] << "\"" ;
    sstr << "\"" << "CuMaxExecCycles"  << "\"" << ":"
	 << "\"" << CuMaxExecCycles[i] << "\"" ;
    sstr << "\"" << "CuStartCount"  << "\"" << ":"
	 << "\"" << CuStartCount[i] << "\"" ;
    sstr << "}" ;
  }
  sstr << "]" ;

  return sstr.str();
}

std::string
am_debug_view::getXGDBString(bool aVerbose) {
  std::stringstream sstr;
  std::vector<std::string> slotNames;
  getIPCountAddrNames(SysfsPath, ACCEL_MONITOR, nullptr, &slotNames);
  int col = 11;
  std::for_each(slotNames.begin(), slotNames.end(), [&](std::string& slotName){
    col = std::max(col, (int)slotName.length() + 4);
  });


  sstr << "Accelerator Monitor (AM) Counters\n" ;
  sstr << std::left
       <<         std::setw(col) << "CU Name"
       << "  " << std::setw(16) << "Exec Count"
       << "  " << std::setw(16) << "Exec Cycles"
       << "  " << std::setw(16) << "Busy Cycles"
       << "  " << std::setw(16) << "Max Parallels"
       << "  " << std::setw(16) << "Ext Stall Cycles"
       << "  " << std::setw(16) << "Int Stall Cycles"
       << "  " << std::setw(16) << "Str Stall Cycles"
       << "  " << std::setw(16) << "Min Exec Cycles"
       << "  " << std::setw(16) << "Max Exec Cycles"
       << "  " << std::setw(16) << "Start Count"
       << std::endl ;
  for (unsigned int i = 0 ; i < NumSlots ; ++i)
  {
    unsigned long long minCycle = (CuMinExecCycles[i] == 0xFFFFFFFFFFFFFFFF) ? 0 : CuMinExecCycles[i];
    sstr << std::left
	 <<         std::setw(col) << slotNames[i]
	 << "  " << std::setw(16) << CuExecCount[i]
     << "  " << std::setw(16) << CuExecCycles[i]
     << "  " << std::setw(16) << CuBusyCycles[i]
     << "  " << std::setw(16) << CuMaxParallelIter[i]
	 << "  " << std::setw(16) << CuStallExtCycles[i]
	 << "  " << std::setw(16) << CuStallIntCycles[i]
   << "  " << std::setw(16) << CuStallStrCycles[i]
   << "  " << std::setw(16) << minCycle
   << "  " << std::setw(16) << CuMaxExecCycles[i]
   << "  " << std::setw(16) << CuStartCount[i]
	 << std::endl ;
  }

  return sstr.str() ;
}

app_debug_view<am_debug_view>*
clGetDebugAccelMonitorCounters()
{
  // Check for error conditions where we cannot read the streaming counters
  if (isEmulationMode()) {
    auto adv = new app_debug_view<am_debug_view>(nullptr, nullptr, true, "xstatus is not supported in emulation flow");
    return adv;
  }
  if (!appdebug::active()) {
    auto adv = new app_debug_view<am_debug_view>(nullptr, nullptr, true, "Runtime instance not yet created");
    return adv;
  }

  xdp::AMCounterResults amCounters;
  memset(&amCounters, 0, sizeof(xdp::AMCounterResults));

  std::string subdev = "icap";
  std::string entry = "debug_ip_layout";
  std::string sysfs_open_path;
  auto platform = appdebug::getcl_platform_id();
  for (auto device : platform->get_device_range())
  {
    if (device->is_active())
    {
      // At this point, we are dealing with only one device
      device->get_xdevice()->debugReadIPStatus(XCL_DEBUG_READ_TYPE_AM, &amCounters);
      sysfs_open_path = device->get_xdevice()->getSysfsPath(subdev, entry).get();
      // ret |= xdp::profile::device::debugReadIPStatus(device, XCL_DEBUG_READ_TYPE_AM, &amCounters);
    }
  }

  auto am_view = new am_debug_view() ;
  am_view->SysfsPath = sysfs_open_path;

  std::copy(amCounters.CuExecCount,
	    amCounters.CuExecCount+xdp::MAX_NUM_AMS,
	    am_view->CuExecCount);
  std::copy(amCounters.CuExecCycles,
	    amCounters.CuExecCycles+xdp::MAX_NUM_AMS,
	    am_view->CuExecCycles);
  std::copy(amCounters.CuBusyCycles,
	    amCounters.CuBusyCycles+xdp::MAX_NUM_AMS,
	    am_view->CuBusyCycles);
  std::copy(amCounters.CuMaxParallelIter,
	    amCounters.CuMaxParallelIter+xdp::MAX_NUM_AMS,
	    am_view->CuMaxParallelIter);
  std::copy(amCounters.CuStallExtCycles,
	    amCounters.CuStallExtCycles+xdp::MAX_NUM_AMS,
	    am_view->CuStallExtCycles);
  std::copy(amCounters.CuStallIntCycles,
	    amCounters.CuStallIntCycles+xdp::MAX_NUM_AMS,
	    am_view->CuStallIntCycles);
  std::copy(amCounters.CuStallStrCycles,
	    amCounters.CuStallStrCycles+xdp::MAX_NUM_AMS,
	    am_view->CuStallStrCycles);
  std::copy(amCounters.CuMinExecCycles,
	    amCounters.CuMinExecCycles+xdp::MAX_NUM_AMS,
	    am_view->CuMinExecCycles);
  std::copy(amCounters.CuMaxExecCycles,
	    amCounters.CuMaxExecCycles+xdp::MAX_NUM_AMS,
	    am_view->CuMaxExecCycles);
  std::copy(amCounters.CuStartCount,
	    amCounters.CuStartCount+xdp::MAX_NUM_AMS,
	    am_view->CuStartCount);

  am_view->NumSlots    = amCounters.NumSlots ;
  am_view->DevUserName = amCounters.DevUserName ;

  auto adv = new app_debug_view<am_debug_view>(am_view, [am_view]() { delete am_view;}, false, "") ;

  return adv ;
}
// End of Accel monitor


struct lapc_debug_view {
  unsigned int   OverallStatus[xdp::MAX_NUM_LAPCS];
  unsigned int   CumulativeStatus[xdp::MAX_NUM_LAPCS][4];
  unsigned int   SnapshotStatus[xdp::MAX_NUM_LAPCS][4];
  unsigned int   NumSlots;
  std::string    DevUserName;
  std::string    SysfsPath;
  lapc_debug_view () {
    std::fill (OverallStatus, OverallStatus + xdp::MAX_NUM_LAPCS, 0);
    for (auto i = 0; i < xdp::MAX_NUM_LAPCS; ++i)
      std::fill (CumulativeStatus[i], CumulativeStatus[i]+4, 0);
    for (auto i = 0; i < xdp::MAX_NUM_LAPCS; ++i)
      std::fill (SnapshotStatus[i], SnapshotStatus[i]+4, 0);
    NumSlots = 0;
    DevUserName = "";
  }
  ~lapc_debug_view() {}
  std::string getstring(int aVerbose = 0, int aJSONFormat = 0);
};

std::string
lapc_debug_view::getstring(int aVerbose, int aJSONFormat) {
  std::stringstream sstr;
  std::vector<std::string> lapcSlotNames;
  std::vector< std::pair<std::string, std::string> > cuNameportNames;

  std::string quotes;
  if (aJSONFormat) quotes = "\"";
  else  quotes = "";

  if (NumSlots == 0) {
    if (aJSONFormat) {
      return "[]";
    }
    else {
      sstr << "No Light Weight AXI Protocol Checker (LAPC) found on the platform \n";
      return sstr.str();
    }
  }

  // unsigned int numSlots =
  getIPCountAddrNames(SysfsPath, LAPC, nullptr, &lapcSlotNames);
  std::pair<size_t, size_t> widths = getCUNamePortName(lapcSlotNames, cuNameportNames);

  bool violations_found = false;
  bool invalid_codes = false;

  if (aJSONFormat) {
    sstr << "["; //spm list
      for (unsigned int i = 0; i<NumSlots; ++i) {
         sstr << (i > 0 ? "," : "") << "{";
         sstr << quotes << "CUName" << quotes << " : " << quotes << cuNameportNames[i].first << quotes << ",";
         sstr << quotes << "AXIPortname" << quotes << " : " << quotes << cuNameportNames[i].second << quotes << ",";
         if (!xdp::isValidAXICheckerCodes(OverallStatus[i],
                               SnapshotStatus[i], CumulativeStatus[i])) {
           sstr << quotes << "FirstViolation" << quotes << " : " << quotes << "Invalid Codes" << quotes << ",";
           sstr << quotes << "OtherViolations" << quotes << " : " << quotes << "Invalid Codes" << quotes ;
         }
         else {
           if (OverallStatus[i]) {
              std::string tstr;
              unsigned int tCummStatus[4] = {0};
              //snapshot reflects first violation, cumulative has all violations
              tstr = xdp::decodeAXICheckerCodes(SnapshotStatus[i]);
              tstr = (tstr == "") ? "None" : tstr;
              sstr << quotes << "FirstViolation" << quotes << " : " << quotes << tstr << quotes << ",";

              std::transform(CumulativeStatus[i], CumulativeStatus[i]+4, SnapshotStatus[i], tCummStatus, std::bit_xor<unsigned int>());
              tstr = xdp::decodeAXICheckerCodes(tCummStatus);
              tstr = (tstr == "") ? "None" : tstr;
              sstr << quotes << "OtherViolations" << quotes << " : " << quotes << tstr << quotes ;
           }
           else {
             sstr << quotes << "FirstViolation" << quotes << " : " << quotes << "None" << quotes << ",";
             sstr << quotes << "OtherViolations" << quotes << " : " << quotes << "None" << quotes ;
           }
         }
         sstr << "}";
      }//for
    sstr << "]";
  }
  else {
    auto col1 = std::max(widths.first, strlen("CU Name")) + 4;
    auto col2 = std::max(widths.second, strlen("AXI Portname"));

    sstr << "Light-weight AXI protocol checker (LAPC) status\n";
    for (unsigned int i = 0; i<NumSlots; ++i) {
      if (!xdp::isValidAXICheckerCodes(OverallStatus[i],
                          SnapshotStatus[i], CumulativeStatus[i])) {
        sstr << "CU Name: " << cuNameportNames[i].first << " AXI Port: " << cuNameportNames[i].second << "\n";
        sstr << "  Invalid codes read, skip decoding\n";
        invalid_codes = true;
      }
      else if (OverallStatus[i]) {
        sstr << "CU Name: " << cuNameportNames[i].first << " AXI Port: " << cuNameportNames[i].second << "\n";
        sstr << "  First violation: \n";
        sstr << "    " <<  xdp::decodeAXICheckerCodes(SnapshotStatus[i]);
        //snapshot reflects first violation, cumulative has all violations
        unsigned int tCummStatus[4] = {0};
        std::transform(CumulativeStatus[i], CumulativeStatus[i]+4, SnapshotStatus[i], tCummStatus, std::bit_xor<unsigned int>());
        sstr << "  Other violations: \n";
        std::string tstr = xdp::decodeAXICheckerCodes(tCummStatus);
        if (tstr == "") {
          sstr << "    " << "None";
        }
        else {
          sstr << "    " <<  tstr;
        }
        violations_found = true;
      }
    }
    if (!violations_found && !invalid_codes) {
      sstr << "No AXI violations found \n";
    }
    if (violations_found && aVerbose && !invalid_codes) {
      sstr << "\n";
      sstr << std::left
                << std::setw(col1) << "CU Name"
                << " " << std::setw(col2) << "AXI Portname"
                << "  " << std::setw(16) << "Overall Status"
                << "  " << std::setw(16) << "Snapshot[0]"
                << "  " << std::setw(16) << "Snapshot[1]"
                << "  " << std::setw(16) << "Snapshot[2]"
                << "  " << std::setw(16) << "Snapshot[3]"
                << "  " << std::setw(16) << "Cumulative[0]"
                << "  " << std::setw(16) << "Cumulative[1]"
                << "  " << std::setw(16) << "Cumulative[2]"
                << "  " << std::setw(16) << "Cumulative[3]"
                << std::endl;
      for (unsigned int i = 0; i<NumSlots; ++i) {
        sstr << std::left
                << std::setw(col1) << cuNameportNames[i].first
                << " " << std::setw(col2) << cuNameportNames[i].second
                << std::hex
                << "  " << std::setw(16) << OverallStatus[i]
                << "  " << std::setw(16) << SnapshotStatus[i][0]
                << "  " << std::setw(16) << SnapshotStatus[i][1]
                << "  " << std::setw(16) << SnapshotStatus[i][2]
                << "  " << std::setw(16) << SnapshotStatus[i][3]
                << "  " << std::setw(16) << CumulativeStatus[i][0]
                << "  " << std::setw(16) << CumulativeStatus[i][1]
                << "  " << std::setw(16) << CumulativeStatus[i][2]
                << "  " << std::setw(16) << CumulativeStatus[i][3]
                << std::dec << std::endl;
      }
    }
  }
  return sstr.str();
}
app_debug_view<lapc_debug_view>*
clGetDebugCheckers() {
  xdp::LAPCCounterResults debugCheckers;

  if (isEmulationMode()) {
    auto adv = new app_debug_view<lapc_debug_view>(nullptr, nullptr, true, "xstatus is not supported in emulation flow");
    return adv;
  }
  if (!appdebug::active()) {
    auto adv = new app_debug_view<lapc_debug_view>(nullptr, nullptr, true, "Runtime instance not yet created");
    return adv;
  }

  std::string subdev = "icap";
  std::string entry = "debug_ip_layout";
  std::string sysfs_open_path;
  auto platform = appdebug::getcl_platform_id();
  // Iterates over all devices, but assumes only one device
  memset(&debugCheckers,0, sizeof(xdp::LAPCCounterResults));
  for (auto device : platform->get_device_range()) {
    if (device->is_active()) {
      //memset(&debugCheckers,0, sizeof(xdp::LAPCCounterResults));
      //At this point we deal with only one deviceyy
      device->get_xdevice()->debugReadIPStatus(XCL_DEBUG_READ_TYPE_LAPC, &debugCheckers);
      sysfs_open_path = device->get_xdevice()->getSysfsPath(subdev, entry).get();
      //ret |= xdp::profile::device::debugReadIPStatus(device, XCL_DEBUG_READ_TYPE_LAPC, &debugCheckers);
    }
  }

  auto lapc_view = new lapc_debug_view ();
  std::copy(debugCheckers.OverallStatus, debugCheckers.OverallStatus+xdp::MAX_NUM_LAPCS, lapc_view->OverallStatus);
  for (auto i = 0; i < xdp::MAX_NUM_LAPCS; ++i)
    std::copy(debugCheckers.CumulativeStatus[i], debugCheckers.CumulativeStatus[i]+4, lapc_view->CumulativeStatus[i]);
  for (auto i = 0; i < xdp::MAX_NUM_LAPCS; ++i)
    std::copy(debugCheckers.SnapshotStatus[i], debugCheckers.SnapshotStatus[i]+4, lapc_view->SnapshotStatus[i]);
  lapc_view->NumSlots = debugCheckers.NumSlots;
  lapc_view->DevUserName = debugCheckers.DevUserName;
  lapc_view->SysfsPath = sysfs_open_path;
  auto adv = new app_debug_view <lapc_debug_view> (lapc_view, [lapc_view](){delete lapc_view;}, false, "");
  return adv;
}
}//appdebug
//Debug functions
