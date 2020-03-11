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

#ifndef xocl_core_execution_context_h
#define xocl_core_execution_context_h

#include "xocl/config.h"
#include "xocl/core/kernel.h"
#include "xocl/core/compute_unit.h"

#include "xrt/scheduler/command.h"
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
 * xrt::command objects to the xrt::scheduler.  The scheduler
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
  struct start_kernel;
  struct start_kernel_conformance;

public:
  using command_type = std::shared_ptr<xrt::command>;
  using packet_type = xrt::command::packet_type;
  using regmap_type = packet_type;
  using word_type = packet_type::word_type;

  using size = std::size_t;
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

  // Kernel state
  using argument_vector_type = std::vector<std::unique_ptr<xocl::kernel::argument>>;
  using argument_iterator_type = argument_vector_type::const_iterator;
  argument_vector_type m_kernel_args;

  bool m_dataflow = false;

  // The context maintains a list of kernel compute units represented
  // by xcl::cu.  These cus (their base addresses) are used in the command
  // that starts the mbs.
  std::vector<const compute_unit*> m_cus;

  // Number of active start_kernel commands in this context
  size_t m_active = 0;

  // Flag to indicate the execution context has no more work
  // to be scheduled
  bool m_done = false;

  std::mutex m_mutex;

  /**
   * Add the device's matching compute units
   */
  void
  add_compute_units(xocl::device* device);

  bool
  write(const command_type& cmd);

  void
  encode_compute_units(packet_type& pkt);

  /**
   * Control type, IP_CONTROL per xclbin ip_layout
   */
  uint32_t
  cu_control_type() const;

  /**
   * Update workgroup accounting.
   */
  void
  update_work();

  void
  start();

  /**
   * Callback to indicate a start_kernel command is done.
   *
   * @return true if execution context is done, false otherwise.
   *   A return value of true, implies that this context can no longer
   *   be used.
   */
  bool
  done(const xrt::command* cmd);

public:
  /**
   * Construct the execution context
   *
   * This is called indirectly from clEnqueuNDRangeKernel via the
   * cl_event::set_execution_context (api/event.cpp)
   *
   * The contructor creates objects of the embedded compute_unit
   * class and adds these to the xrt infrastruture.
   */
  execution_context(device* device
                    ,kernel* kd
                    ,event* event
                    ,size_t work_dim
                    ,const size_t* global_work_offset
                    ,const size_t* global_work_size
                    ,const size_t* local_work_size);

  unsigned long
  get_uid() const
  {
    return m_uid;
  }

  const size_t*
  get_global_work_size() const
  { return m_gsize.data(); }

  size_t
  get_global_work_size(unsigned int d) const
  { return m_gsize[d]; }

  const size_t*
  get_local_work_size() const
  { return m_lsize.data(); }

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

  xocl::range<argument_iterator_type>
  get_indexed_argument_range() const
  {
    return m_kernel->get_indexed_argument_range();
  }

  xocl::range<argument_iterator_type>
  get_progvar_argument_range() const
  {
    return m_kernel->get_progvar_argument_range();
  }

  /**
   * Get the kernel for this context
   *
   * @return
   *  Raw pointer to kernel object
   */
  const kernel*
  get_kernel() const
  {
    return m_kernel.get();
  }

  /**
   * Get the kernel event associated with this context
   *
   * @return
   *   The event associated with this execution_context
   */
  const event*
  get_event() const
  {
    return m_event;
  }

  /**
   * Translate a cu index into a compute_unit object
   *
   * @param cu_idx
   *   Index of cu
   * @return
   *   Pointer to compute unit object that maps to cu_idx, nullptr
   *   if no mapping
   */
  XRT_XOCL_EXPORT
  const compute_unit*
  get_compute_unit(unsigned int cu_idx) const;

  /**
   * Start execution context.
   *
   * The context is started through event trigger action as
   * soon as an event changes state to CL_SUBMITTED.
   *
   * The context creates scheduler commands and submits
   * these commands to the command queue.
   *
   * @return
   *    true if last work group was started, false otherwise
   */
  bool
  execute();

private:
  // Call back for start_kernel_conformance comands
  bool
  conformance_done(const xrt::command* cmd);

  // Execute a context in conformance mode
  bool
  conformance_execute();
};

/**
 * Callback function type for kernel command callbacks
 */
using command_callback_function_type = std::function<void(const xrt::command*,const execution_context*)>;

/**
 * Register function to invoke when a kernel command is constructed
 */
XRT_XOCL_EXPORT
void
add_command_start_callback(command_callback_function_type fcn);

/**
 * Register function to invoke when a kernel command completes
 */
XRT_XOCL_EXPORT
void
add_command_done_callback(command_callback_function_type fcn);


} // xocl

#endif
