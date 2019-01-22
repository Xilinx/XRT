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
 * This file contains the implementation of the application debug
 * It exposes a set of functions that are callable from debugger(GDB)
 * It defines data structures that provide the view of runtime data structures such as cl_event and cl_meme
 * It defines lambda functions that are attached as debug action with the cl_event
 */
#include "appdebug.h"
#include "appdebug_track.h"
#include "xdp/rt_singleton.h"
#include "xdp/profile/core/rt_profile.h"

#include "xocl/core/event.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/device.h"
#include "xocl/core/platform.h"
#include "xocl/core/context.h"
#include "xocl/core/compute_unit.h"
#include "xocl/core/execution_context.h"

#include "xclbin/binary.h"
#include "impl/spir.h"

#include "xclperf.h"
#include "xcl_app_debug.h"
#include "xclbin.h"
#include "xcl_axi_checker_codes.h"

#include <map>
#include <sstream>
#include <fstream>
#include "xocl/api/plugin/xdp/appdebug.h"

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
    catch (const xocl::error & ex) {
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
void cb_scheduler_cmd_start (const xrt::command* aCommand, const xocl::execution_context* aContext)
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
void cb_scheduler_cmd_done (const xrt::command* aCommand, const xocl::execution_context* aContext)
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
template class app_debug_view<spm_debug_view>;
template class app_debug_view<sspm_debug_view>;
template class app_debug_view<lapc_debug_view>;
template class app_debug_view<std::vector<cl_command_queue>>;
template class app_debug_view<std::vector<cl_mem>>;

template class app_debug_track<cl_command_queue>;
template class app_debug_track<cl_mem>;

//Initialize the static member of app_debug_track
template<> bool app_debug_track<cl_command_queue>::m_set = true;
template<> bool app_debug_track<cl_mem>::m_set = true;
bool app_debug_track<cl_event>::m_set = true;


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

    mdv = new clmem_debug_view(aMem, xoclMem->get_uid(), "Unknown", -1,
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

static
std::string
getArgValueString(const xocl::event* aEvent)
{
  std::stringstream sstr;
  auto ctx = aEvent->get_execution_context();
  for (auto& arg : ctx->get_indexed_argument_range()) {
    auto address_space = arg->get_address_space();
    if (address_space == SPIR_ADDRSPACE_PRIVATE)
    {
      //auto arginforange = arg->get_arginfo_range();
      //sstr << arg->get_name() << " = " << getscalarval((const void*)arg->get_value(), arg->get_size(),arginforange) << " ";
      sstr << arg->get_name() << " = " << arg->get_string_value() << " ";
    } else if (address_space==SPIR_ADDRSPACE_PIPES){
      sstr << arg->get_name() << " = " << "stream arg " << std::dec;

    } else if (address_space==SPIR_ADDRSPACE_GLOBAL
             || address_space==SPIR_ADDRSPACE_CONSTANT)
    {
      uint64_t physaddr = 0;
      std::string bank = "";
      if (auto mem = arg->get_memory_object()) {
        xocl::xocl(mem)->try_get_address_bank(physaddr, bank);
      }

      sstr << arg->get_name() << " = 0x" << std::hex << physaddr << std::dec << "(Bank-";
      sstr << bank;
      sstr <<  ") ";
    }
    else if (address_space==SPIR_ADDRSPACE_LOCAL)
    {
      sstr << arg->get_name() << " = " << "local arg " << std::dec;
    }
  }

  //Now collect the progvars
  size_t eventargitint = 0;

  for (auto& arg : ctx->get_progvar_argument_range()) {
    uint64_t physaddr = 0;
    std::string bank;
    if (eventargitint == 0) sstr << "ProgVars: ";
    if (auto mem = arg->get_memory_object()) {
      xocl::xocl(mem)->try_get_address_bank(physaddr, bank);
    }
    //progvars are prefixed "__xcl_gv_", remove them before printing
    std::string argname = arg->get_name();
    std::string progvar_prefix = "__xcl_gv_";
    if (argname.find(progvar_prefix)!= std::string::npos) {
      argname = argname.substr(progvar_prefix.length());
    }
    sstr << argname <<  " = 0x" << std::hex << physaddr << " " << std::dec;
    ++eventargitint;
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
  return xrt::config::get_app_debug();
}

uint32_t getIPCountAddrNames(std::string& devUserName, int type, std::vector<uint64_t> *baseAddress, std::vector<std::string> * portNames) {
  debug_ip_layout *map;
  //Get the path to the device from the HAL
  std::string path = "/sys/bus/pci/devices/" + devUserName + "/debug_ip_layout";
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
    for (auto slotName: aSlotNames) {
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
        //Replace the name of the host-spm to something simple
        if (aCUNamePortNames.back().first.find("interconnect_host_aximm") != std::string::npos) {
            aCUNamePortNames.pop_back();
            aCUNamePortNames.emplace_back("XDMA", "N/A");
        }
        max1 = std::max(aCUNamePortNames.back().first.length(), max1);
        max2 = std::max(aCUNamePortNames.back().second.length(), max2);
    }
    return std::pair<size_t, size_t>(max1, max2);
}

struct spm_debug_view {
  unsigned int   WriteBytes[XSPM_MAX_NUMBER_SLOTS];
  unsigned int   WriteTranx[XSPM_MAX_NUMBER_SLOTS];
  unsigned int   ReadBytes[XSPM_MAX_NUMBER_SLOTS];
  unsigned int   ReadTranx[XSPM_MAX_NUMBER_SLOTS];

  unsigned int   OutStandCnts[XSPM_MAX_NUMBER_SLOTS];
  unsigned int   LastWriteAddr[XSPM_MAX_NUMBER_SLOTS];
  unsigned int   LastWriteData[XSPM_MAX_NUMBER_SLOTS];
  unsigned int   LastReadAddr[XSPM_MAX_NUMBER_SLOTS];
  unsigned int   LastReadData[XSPM_MAX_NUMBER_SLOTS];
  unsigned int   NumSlots;
  std::string    DevUserName;
  spm_debug_view () {
    std::fill (WriteBytes, WriteBytes+XSPM_MAX_NUMBER_SLOTS, 0);
    std::fill (WriteTranx, WriteTranx+XSPM_MAX_NUMBER_SLOTS, 0);
    std::fill (WriteBytes, WriteBytes+XSPM_MAX_NUMBER_SLOTS, 0);
    std::fill (ReadTranx, ReadTranx+XSPM_MAX_NUMBER_SLOTS, 0);
    std::fill (OutStandCnts, OutStandCnts+XSPM_MAX_NUMBER_SLOTS, 0);
    std::fill (LastWriteAddr, LastWriteAddr+XSPM_MAX_NUMBER_SLOTS, 0);
    std::fill (LastWriteData, LastWriteData+XSPM_MAX_NUMBER_SLOTS, 0);
    std::fill (LastReadAddr, LastReadAddr+XSPM_MAX_NUMBER_SLOTS, 0);
    std::fill (LastReadData, LastReadData+XSPM_MAX_NUMBER_SLOTS, 0);
    NumSlots = 0;
    DevUserName = "";
  }
  ~spm_debug_view() {}
  std::string getstring(int aVerbose = 0, int aJSONFormat = 0);
};

std::string
spm_debug_view::getstring(int aVerbose, int aJSONFormat) {
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
      sstr << "No SPM found on the platform \n";
      return sstr.str();
    }
  }

  //  unsigned int numSlots = 
  getIPCountAddrNames (DevUserName, AXI_MM_MONITOR, nullptr, &slotNames);
  std::pair<size_t, size_t> widths = getCUNamePortName(slotNames, cuNameportNames);

  if (aJSONFormat) {
    sstr << "["; //spm list
      for (unsigned int i = 0; i<NumSlots; ++i) {
         sstr << (i > 0 ? "," : "") << "{";
         sstr << quotes << "CUName" << quotes << " : " << quotes << cuNameportNames[i].first << quotes << ",";
         sstr << quotes << "AXIPortname" << quotes << " : " << quotes << cuNameportNames[i].second << quotes << ",";
         sstr << quotes << "WriteBytes" << quotes << " : " << quotes <<  WriteBytes[i] << quotes << ",";
         sstr << quotes << "WriteTranx" << quotes << " : " << quotes <<  WriteTranx[i] << quotes << ",";
         sstr << quotes << "ReadBytes" << quotes << " : " << quotes <<  ReadBytes[i] << quotes << ",";
         sstr << quotes << "ReadTranx" << quotes << " : " << quotes <<  ReadTranx[i] << quotes << ",";
         sstr << quotes << "OutstandingCnt" << quotes << " : " << quotes <<  OutStandCnts[i] << quotes << ",";
         sstr << quotes << "LastWrAddr" << quotes << " : " << quotes << std::hex << "0x" <<  LastWriteAddr[i] << std::dec << quotes << ",";
         sstr << quotes << "LastWrData" << quotes << " : " << quotes <<  LastWriteData[i] << quotes << ",";
         sstr << quotes << "LastRdAddr" << quotes << " : " << quotes << std::hex << "0x" <<  LastReadAddr[i] << std::dec << quotes << ",";
         sstr << quotes << "LastRdData" << quotes << " : " << quotes <<  LastReadData[i] << quotes ;
         sstr << "}";
      }
    sstr << "]";
  }
  else {
    sstr<< "SDx Performance Monitor Counters\n";
    int col1 = std::max(widths.first, strlen("CU Name")) + 4;
    int col2 = std::max(widths.second, strlen("AXI Portname"));

    sstr << std::left
              << std::setw(col1) << "CU Name"
              << " " << std::setw(col2) << "AXI Portname"
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
              << " " << std::setw(col2) << cuNameportNames[i].second
              << "  " << std::setw(16) << WriteBytes[i]
              << "  " << std::setw(16) << WriteTranx[i]
              << "  " << std::setw(16) << ReadBytes[i]
              << "  " << std::setw(16) << ReadTranx[i]
              << "  " << std::setw(16) << OutStandCnts[i]
              << "  " << std::hex << "0x" << std::setw(16) << LastWriteAddr[i] << std::dec
              << "  " << std::setw(16) << LastWriteData[i]
              << "  " << std::hex << "0x" << std::setw(16) << LastReadAddr[i] << std::dec
              << "  " << std::setw(16) << LastReadData[i]
              << std::endl;
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

app_debug_view<spm_debug_view>*
clGetDebugCounters() {
  cl_int ret = CL_SUCCESS;
  xclDebugCountersResults debugResults = {0};

  if (isEmulationMode()) {
    auto adv = new app_debug_view<spm_debug_view>(nullptr, nullptr, true, "xstatus is not supported in emulation flow");
    return adv;
  }

  if (!xdp::active()) {
    auto adv = new app_debug_view<spm_debug_view>(nullptr, nullptr, true, "Runtime instance not yet created");
    return adv;
  }
  auto rts = xdp::RTSingleton::Instance();
  if (!rts) {
    auto adv = new app_debug_view<spm_debug_view>(nullptr, nullptr, true, "Error: Runtime instance not available");
    return adv;
  }

  auto platform = rts->getcl_platform_id();
  // Iterates over all devices, but assumes only one device
  memset(&debugResults,0, sizeof(xclDebugCountersResults));
  for (auto device : platform->get_device_range()) {
    if (device->is_active()) {
      //memset(&debugResults,0, sizeof(xclDebugCountersResults));
      //At this point we deal with only one deviceyy
      device->get_xrt_device()->debugReadIPStatus(XCL_DEBUG_READ_TYPE_SPM, &debugResults);
      //ret |= xdp::profile::device::debugReadIPStatus(device, XCL_DEBUG_READ_TYPE_SPM, &debugResults);
    }
  }

  if (ret) {
    auto adv = new app_debug_view<spm_debug_view>(nullptr, nullptr, true, "Error reading spm counters");
    return adv;
  }
  auto spm_view = new spm_debug_view ();
  std::copy(debugResults.WriteBytes, debugResults.WriteBytes+XSPM_MAX_NUMBER_SLOTS, spm_view->WriteBytes);
  std::copy(debugResults.WriteTranx, debugResults.WriteTranx+XSPM_MAX_NUMBER_SLOTS, spm_view->WriteTranx);
  std::copy(debugResults.ReadBytes, debugResults.ReadBytes+XSPM_MAX_NUMBER_SLOTS, spm_view->ReadBytes);
  std::copy(debugResults.ReadTranx, debugResults.ReadTranx+XSPM_MAX_NUMBER_SLOTS, spm_view->ReadTranx);
  std::copy(debugResults.OutStandCnts, debugResults.OutStandCnts+XSPM_MAX_NUMBER_SLOTS, spm_view->OutStandCnts);
  std::copy(debugResults.LastWriteAddr, debugResults.LastWriteAddr+XSPM_MAX_NUMBER_SLOTS, spm_view->LastWriteAddr);
  std::copy(debugResults.LastWriteData, debugResults.LastWriteData+XSPM_MAX_NUMBER_SLOTS, spm_view->LastWriteData);
  std::copy(debugResults.LastReadAddr, debugResults.LastReadAddr+XSPM_MAX_NUMBER_SLOTS, spm_view->LastReadAddr);
  std::copy(debugResults.LastReadData, debugResults.LastReadData+XSPM_MAX_NUMBER_SLOTS, spm_view->LastReadData);
  spm_view->NumSlots = debugResults.NumSlots;
  spm_view->DevUserName = debugResults.DevUserName;

  auto adv = new app_debug_view <spm_debug_view> (spm_view, [spm_view](){delete spm_view;}, false, "");
  return adv;
}

// Streaming counter view

struct sspm_debug_view {
  unsigned long long int StrNumTranx    [XSSPM_MAX_NUMBER_SLOTS];
  unsigned long long int StrDataBytes   [XSSPM_MAX_NUMBER_SLOTS];
  unsigned long long int StrBusyCycles  [XSSPM_MAX_NUMBER_SLOTS];
  unsigned long long int StrStallCycles [XSSPM_MAX_NUMBER_SLOTS];
  unsigned long long int StrStarveCycles[XSSPM_MAX_NUMBER_SLOTS];

  unsigned int NumSlots ;
  std::string  DevUserName ;

  sspm_debug_view() 
  {
    std::fill(StrNumTranx,     StrNumTranx     + XSSPM_MAX_NUMBER_SLOTS, 0);
    std::fill(StrDataBytes,    StrDataBytes    + XSSPM_MAX_NUMBER_SLOTS, 0);
    std::fill(StrBusyCycles,   StrBusyCycles   + XSSPM_MAX_NUMBER_SLOTS, 0);
    std::fill(StrStallCycles,  StrStallCycles  + XSSPM_MAX_NUMBER_SLOTS, 0);
    std::fill(StrStarveCycles, StrStarveCycles + XSSPM_MAX_NUMBER_SLOTS, 0);

    NumSlots = 0;
  }
  ~sspm_debug_view() { }
  std::string getstring(int aVerbose = 0, int aJSONFormat = 0);
  
  std::string getJSONString(bool aVerbose) ;
  std::string getXGDBString(bool aVerbose) ;
} ;

std::string
sspm_debug_view::getstring(int aVerbose, int aJSONFormat) {
  if (aJSONFormat) return getJSONString(aVerbose != 0 ? true : false) ;  
  else return getXGDBString(aVerbose != 0 ? true : false) ;
}

std::string
sspm_debug_view::getJSONString(bool aVerbose) {
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
sspm_debug_view::getXGDBString(bool aVerbose) {
  std::stringstream sstr;

  sstr << "SDx Streaming Performance Monitor Counters\n" ;
  sstr << std::left
       <<         std::setw(32) << "Number of Transactions"
       << "  " << std::setw(16) << "Data Bytes" 
       << "  " << std::setw(16) << "Busy Cycles" 
       << "  " << std::setw(16) << "Stall Cycles"
       << "  " << std::setw(16) << "Starve Cycles"
       << std::endl ;
  for (unsigned int i = 0 ; i < NumSlots ; ++i)
  {
    sstr << std::left
	 <<         std::setw(32) << StrNumTranx[i] 
	 << "  " << std::setw(16) << StrDataBytes[i]
	 << "  " << std::setw(16) << StrBusyCycles[i]
	 << "  " << std::setw(16) << StrStallCycles[i]
	 << "  " << std::setw(16) << StrStarveCycles[i]
	 << std::endl ;
  }

  return sstr.str() ;
}

app_debug_view<sspm_debug_view>*
clGetDebugStreamCounters()
{
  // Check for error conditions where we cannot read the streaming counters
  if (isEmulationMode()) {
    auto adv = new app_debug_view<sspm_debug_view>(nullptr, nullptr, true, "xstatus is not supported in emulation flow");
    return adv;
  }
  if (!xdp::active()) {
    auto adv = new app_debug_view<sspm_debug_view>(nullptr, nullptr, true, "Runtime instance not yet created");
    return adv;
  }
  auto rts = xdp::RTSingleton::Instance();
  if (!rts) {
    auto adv = new app_debug_view<sspm_debug_view>(nullptr, nullptr, true, "Error: Runtime instance not available");
    return adv;
  }

  cl_int ret = CL_SUCCESS;

  xclStreamingDebugCountersResults streamingDebugCounters;  
  memset(&streamingDebugCounters, 0, sizeof(xclStreamingDebugCountersResults));

  auto platform = rts->getcl_platform_id();
  for (auto device : platform->get_device_range())
  {
    if (device->is_active())
    {
      // At this point, we are dealing with only one device
      device->get_xrt_device()->debugReadIPStatus(XCL_DEBUG_READ_TYPE_SSPM, &streamingDebugCounters);
      //ret |= xdp::profile::device::debugReadIPStatus(device, XCL_DEBUG_READ_TYPE_SSPM, &streamingDebugCounters);
    }
  }

  if (ret) 
  {
    auto adv = new app_debug_view<sspm_debug_view>(nullptr, nullptr, true, "Error reading sspm counters");
    return adv;
  }

  auto sspm_view = new sspm_debug_view () ;
  
  std::copy(streamingDebugCounters.StrNumTranx,
	    streamingDebugCounters.StrNumTranx+XSSPM_MAX_NUMBER_SLOTS,
	    sspm_view->StrNumTranx);
  std::copy(streamingDebugCounters.StrDataBytes,
	    streamingDebugCounters.StrDataBytes+XSSPM_MAX_NUMBER_SLOTS,
	    sspm_view->StrDataBytes);
  std::copy(streamingDebugCounters.StrBusyCycles,
	    streamingDebugCounters.StrBusyCycles+XSSPM_MAX_NUMBER_SLOTS,
	    sspm_view->StrBusyCycles);
  std::copy(streamingDebugCounters.StrStallCycles,
	    streamingDebugCounters.StrStallCycles+XSSPM_MAX_NUMBER_SLOTS,
	    sspm_view->StrStallCycles);
  std::copy(streamingDebugCounters.StrStarveCycles,
	    streamingDebugCounters.StrStarveCycles+XSSPM_MAX_NUMBER_SLOTS,
	    sspm_view->StrStarveCycles);
  
  sspm_view->NumSlots    = streamingDebugCounters.NumSlots ;
  sspm_view->DevUserName = streamingDebugCounters.DevUserName ;

  auto adv = new app_debug_view<sspm_debug_view>(sspm_view, [sspm_view]() { delete sspm_view;}, false, "") ;
  return adv ;
}

struct lapc_debug_view {
  unsigned int   OverallStatus[XLAPC_MAX_NUMBER_SLOTS];
  unsigned int   CumulativeStatus[XLAPC_MAX_NUMBER_SLOTS][4];
  unsigned int   SnapshotStatus[XLAPC_MAX_NUMBER_SLOTS][4];
  unsigned int   NumSlots;
  std::string    DevUserName;
  lapc_debug_view () {
    std::fill (OverallStatus, OverallStatus+XLAPC_MAX_NUMBER_SLOTS, 0);
    for (auto i = 0; i<XLAPC_MAX_NUMBER_SLOTS; ++i)
      std::fill (CumulativeStatus[i], CumulativeStatus[i]+4, 0);
    for (auto i = 0; i<XLAPC_MAX_NUMBER_SLOTS; ++i)
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
      sstr << "No LAPC found on the platform \n";
      return sstr.str();
    }
  }

  // unsigned int numSlots = 
  getIPCountAddrNames (DevUserName, LAPC, nullptr, &lapcSlotNames);
  std::pair<size_t, size_t> widths = getCUNamePortName(lapcSlotNames, cuNameportNames);

  bool violations_found = false;
  bool invalid_codes = false;

  if (aJSONFormat) {
    sstr << "["; //spm list
      for (unsigned int i = 0; i<NumSlots; ++i) {
         sstr << (i > 0 ? "," : "") << "{";
         sstr << quotes << "CUName" << quotes << " : " << quotes << cuNameportNames[i].first << quotes << ",";
         sstr << quotes << "AXIPortname" << quotes << " : " << quotes << cuNameportNames[i].second << quotes << ",";
         if (!xclAXICheckerCodes::isValidAXICheckerCodes(OverallStatus[i],
                               SnapshotStatus[i], CumulativeStatus[i])) {
           sstr << quotes << "FirstViolation" << quotes << " : " << quotes << "Invalid Codes" << quotes << ",";
           sstr << quotes << "OtherViolations" << quotes << " : " << quotes << "Invalid Codes" << quotes ;
         }
         else {
           if (OverallStatus[i]) {
              std::string tstr;
              unsigned int tCummStatus[4];
              //snapshot reflects first violation, cumulative has all violations
              tstr = xclAXICheckerCodes::decodeAXICheckerCodes(SnapshotStatus[i]);
              tstr = (tstr == "") ? "None" : tstr;
              sstr << quotes << "FirstViolation" << quotes << " : " << quotes << tstr << quotes << ",";

              std::transform(CumulativeStatus[i], CumulativeStatus[i]+4, SnapshotStatus[i], tCummStatus, std::bit_xor<unsigned int>());
              tstr = xclAXICheckerCodes::decodeAXICheckerCodes(tCummStatus);
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
    int col1 = std::max(widths.first, strlen("CU Name")) + 4;
    int col2 = std::max(widths.second, strlen("AXI Portname"));

    sstr << "Light-weight AXI protocol checker status\n";
    for (unsigned int i = 0; i<NumSlots; ++i) {
      if (!xclAXICheckerCodes::isValidAXICheckerCodes(OverallStatus[i],
                          SnapshotStatus[i], CumulativeStatus[i])) {
        sstr << "CU Name: " << cuNameportNames[i].first << " AXI Port: " << cuNameportNames[i].second << "\n";
        sstr << "  Invalid codes read, skip decoding\n";
        invalid_codes = true;
      }
      else if (OverallStatus[i]) {
        sstr << "CU Name: " << cuNameportNames[i].first << " AXI Port: " << cuNameportNames[i].second << "\n";
        sstr << "  First violation: \n";
        sstr << "    " <<  xclAXICheckerCodes::decodeAXICheckerCodes(SnapshotStatus[i]);
        //snapshot reflects first violation, cumulative has all violations
        unsigned int tCummStatus[4];
        std::transform(CumulativeStatus[i], CumulativeStatus[i]+4, SnapshotStatus[i], tCummStatus, std::bit_xor<unsigned int>());
        sstr << "  Other violations: \n";
        std::string tstr = xclAXICheckerCodes::decodeAXICheckerCodes(tCummStatus);
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
                << "  " << std::setw(16) << std::hex << OverallStatus[i]
                << "  " << std::setw(16) << std::hex << SnapshotStatus[i][0]
                << "  " << std::setw(16) << std::hex << SnapshotStatus[i][1]
                << "  " << std::setw(16) << std::hex << SnapshotStatus[i][2]
                << "  " << std::setw(16) << std::hex << SnapshotStatus[i][3]
                << "  " << std::setw(16) << std::hex << CumulativeStatus[i][0]
                << "  " << std::setw(16) << std::hex << CumulativeStatus[i][1]
                << "  " << std::setw(16) << std::hex << CumulativeStatus[i][2]
                << "  " << std::setw(16) << std::hex << CumulativeStatus[i][3]
                << std::endl;
      }
    }
  }
  return sstr.str();
}
app_debug_view<lapc_debug_view>*
clGetDebugCheckers() {
  cl_int ret = CL_SUCCESS;
  xclDebugCheckersResults debugCheckers;

  if (isEmulationMode()) {
    auto adv = new app_debug_view<lapc_debug_view>(nullptr, nullptr, true, "xstatus is not supported in emulation flow");
    return adv;
  }
  if (!xdp::active()) {
    auto adv = new app_debug_view<lapc_debug_view>(nullptr, nullptr, true, "Runtime instance not yet created");
    return adv;
  }
  auto rts = xdp::RTSingleton::Instance();
  if (!rts) {
    auto adv = new app_debug_view<lapc_debug_view>(nullptr, nullptr, true, "Error: Runtime instance not available");
    return adv;
  }
  auto platform = rts->getcl_platform_id();
  // Iterates over all devices, but assumes only one device
  memset(&debugCheckers,0, sizeof(xclDebugCheckersResults));
  for (auto device : platform->get_device_range()) {
    if (device->is_active()) {
      //memset(&debugCheckers,0, sizeof(xclDebugCheckersResults));
      //At this point we deal with only one deviceyy
      device->get_xrt_device()->debugReadIPStatus(XCL_DEBUG_READ_TYPE_LAPC, &debugCheckers);
      //ret |= xdp::profile::device::debugReadIPStatus(device, XCL_DEBUG_READ_TYPE_LAPC, &debugCheckers);
    }
  }

  if (ret) {
    auto adv = new app_debug_view<lapc_debug_view>(nullptr, nullptr, true, "Error reading lapc status");
    return adv;
  }
  auto lapc_view = new lapc_debug_view ();
  std::copy(debugCheckers.OverallStatus, debugCheckers.OverallStatus+XLAPC_MAX_NUMBER_SLOTS, lapc_view->OverallStatus);
  for (auto i = 0; i<XLAPC_MAX_NUMBER_SLOTS; ++i)
    std::copy(debugCheckers.CumulativeStatus[i], debugCheckers.CumulativeStatus[i]+4, lapc_view->CumulativeStatus[i]);
  for (auto i = 0; i<XLAPC_MAX_NUMBER_SLOTS; ++i)
    std::copy(debugCheckers.SnapshotStatus[i], debugCheckers.SnapshotStatus[i]+4, lapc_view->SnapshotStatus[i]);
  lapc_view->NumSlots = debugCheckers.NumSlots;
  lapc_view->DevUserName = debugCheckers.DevUserName;
  auto adv = new app_debug_view <lapc_debug_view> (lapc_view, [lapc_view](){delete lapc_view;}, false, "");
  return adv;
}
}//appdebug
//Debug functions


