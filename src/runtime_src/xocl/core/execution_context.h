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

#ifndef xocl_core_execution_context_h
#define xocl_core_execution_context_h

#include "xocl/config.h"
#include "xocl/core/kernel.h"
#include "xocl/core/compute_unit.h"

#include "core/include/xrt/detail/xclbin.h"
#include "core/include/xrt/experimental/xrt_xclbin.h"

#include <mutex>
#include <array>
#include <algorithm>
#include <iostream>
#include <cassert>

namespace xocl {

class event;

/**
 * Class for xocl::execution_context created via clEnqueueNDRangeKernel
 *
 * An execution context capture everything needed to execute an
 * NDRange kernel event.  The context is owned by an xocl::event and
 * is deleted when the xocl::event destructs.
 *
 * The execution context executes a kernel event by sending
 * xrt_xocl::command objects to the xrt_xocl::scheduler.  The scheduler
 * itself can be running in either software or hardware.
 *
 * When a command is constructed all registered construction callbacks
 * are invoked.  When the command completes, all registered completion
 * callbacks are called before the command itself is deleted.
 *
 * Command ownership is managed by execution context.
 */
class execution_context
{
public:
  using size = std::size_t;
  using size1 = size;
  using size3 = std::array<size,3>;

private:
  unsigned int m_uid {0};

  // The work to be performed by this execution context instance
  // These are the arguments to clEnqueuNDRangeKernel
  size m_dim = 0;
  size3 m_goffset {{0,0,0}};
  size3 m_gsize   {{1,1,1}};
  size3 m_lsize   {{1,1,1}};

  // Current work progress
  size3 m_cu_global_id {{0,0,0}};
  size3 m_cu_group_id  {{0,0,0}};

  // The event that represents the kernel execution.  This is the
  // event created by clEnqueueNDRangeKernel and it is the event that
  // has an owning pointer to this execution_context object.
  event* m_event;

  // The kernel that is executed in this context
  ptr<xocl::kernel> m_kernel;

  // The device associated with this context
  device* m_device;

  // Number of compute units in the run object
  size_t m_num_cus = 0;

  // Control protocol
  xrt::xclbin::ip::control_type m_control = xrt::xclbin::ip::control_type::hs;

  // The kernel run object to be started and managed by this context
  xrt::run m_run;

  // For work-group reuse
  std::vector<xrt::run> m_freeruns;

  // For active runs, to keep a reference to cloned run objects
  // while they are active
  std::map<const void*, xrt::run> m_activeruns;

  // Number of active start_kernel commands in this context
  size_t m_active = 0;

  // Flag to indicate the execution context has no more work
  // to be scheduled
  bool m_done = false;

  std::mutex m_mutex;

  // Set global argument on xrt::run object
  void
  set_global_arg_at_index(xrt::run&, size_t index, const xocl::memory* mem);

  // Set printf specific argument on xrt::run object
  void
  set_rtinfo_printf(xrt::run&, size_t index, const xocl::memory*);

  // Set OpenCL specific runtime argument
  void
  set_rtinfo_arg1(xrt::run&, size_t index, size1);

  // Set OpenCL specific runtime argument
  void
  set_rtinfo_arg3(xrt::run&, size_t index, const size3&);

  // Set OpenCL specific runtime argument
  void
  set_rtinfo_args(xrt::run&);

  // Run object to use for starting work group
  xrt::run
  get_free_run();

  // Mark a run as actice
  void
  mark_active(const xrt::run& run);

  // Mark a run as inactive
  xrt::run
  mark_inactive(const void* key);

  // Update workgroup accounting.
  void
  update_work();

  // Start a workgroup
  void
  start();


public:
  // Callback for completed kernel run execution
  bool
  done(const void*);

public:
  // Construct execution context
  //
  // This is called indirectly from clEnqueuNDRangeKernel via the
  // cl_event::set_execution_context (api/event.cpp)
  execution_context(device* device
                    ,kernel* kd
                    ,event* event
                    ,size_t work_dim
                    ,const size_t* global_work_offset
                    ,const size_t* global_work_size
                    ,const size_t* local_work_size);


  ~execution_context();

  // Unique id of this object
  unsigned long
  get_uid() const
  {
    return m_uid;
  }

  //
  const size_t*
  get_global_work_size() const
  { return m_gsize.data(); }

  //
  size_t
  get_global_work_size(unsigned int d) const
  { return m_gsize[d]; }

  //
  const size_t*
  get_local_work_size() const
  { return m_lsize.data(); }

  //
  size_t
  get_num_work_groups () const
  {
    size_t num = 1;
    for (auto d : {0,1,2}) {
      if (m_lsize[d])
        num *= m_gsize[d]/m_lsize[d];
    }
    return num;
  }

  // Kernel object associated with this context
  const kernel*
  get_kernel() const
  {
    return m_kernel.get();
  }

  // Get the kernel event associated with this context
  const event*
  get_event() const
  {
    return m_event;
  }

  // Start execution context.
  //
  // The context is started through event trigger action as
  // soon as an event changes state to CL_SUBMITTED.
  bool
  execute();
};

// Callback function type for kernel command callbacks
using command_callback_function_type = std::function<void(const execution_context*, const xrt::run&)>;

// Register function to invoke when a kernel command is constructed
XRT_XOCL_EXPORT
void
add_command_start_callback(command_callback_function_type fcn);

// Register function to invoke when a kernel command completes
XRT_XOCL_EXPORT
void
add_command_done_callback(command_callback_function_type fcn);


} // xocl

#endif
