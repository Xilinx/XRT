/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

// This file implements XRT kernel APIs as declared in
// core/include/experimental/xrt_kernel.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_kernel.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_kernel.h"

#include "command.h"
#include "exec.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/xclbin_parser.h"
#include "core/common/bo_cache.h"
#include "core/common/message.h"
#include "core/include/xclbin.h"
#include "core/include/ert.h"
#include <memory>
#include <map>
#include <bitset>
#include <stdexcept>
#include <cstdarg>
#include <type_traits>
#include <utility>
#include <mutex>
#include <condition_variable>

#ifdef _WIN32
# pragma warning( disable : 4244 4267)
#endif

namespace {

// struct device_type - Extends xrt_core::device
//
// This struct is not really needed.
// Data members should be moved to xrt_core::device, but
// some are maintained in shim and not accessible outside
// What's worse is that not all data members are in all shims
struct device_type
{
  std::shared_ptr<xrt_core::device> core_device;
  xrt_core::bo_cache exec_buffer_cache;

  device_type(xrtDeviceHandle dhdl)
    : core_device(xrt_core::get_userpf_device(dhdl))
    , exec_buffer_cache(dhdl, 128)
  {}

  template <typename CommandType>
  xrt_core::bo_cache::cmd_bo<CommandType>
  create_exec_buf()
  {
    return exec_buffer_cache.alloc<CommandType>();
  }

  xrt_core::device*
  get_core_device() const
  {
    return core_device.get();
  }
};

// class kernel_command - Immplements command API expected by schedulers
//
// The kernel command is
class kernel_command : public xrt_core::command
{
public:
  using execbuf_type = xrt_core::bo_cache::cmd_bo<ert_start_kernel_cmd>;
  using callback_function_type = std::function<void(ert_cmd_state)>;
  using callback_list = std::vector<callback_function_type>;

public:
  kernel_command(device_type* dev)
    : m_device(dev)
    , m_execbuf(m_device->create_exec_buf<ert_start_kernel_cmd>())
    , m_done(true)
  {}

  ~kernel_command()
  {
    // This is problematic, bo_cache should return managed BOs
    m_device->exec_buffer_cache.release(m_execbuf);
  }

  /**
   * Cast underlying exec buffer to its requested type
   */
  template <typename ERT_COMMAND_TYPE>
  const ERT_COMMAND_TYPE
  get_ert_cmd() const
  {
    return reinterpret_cast<const ERT_COMMAND_TYPE>(get_ert_packet());
  }

  /**
   * Cast underlying exec buffer to its requested type
   */
  template <typename ERT_COMMAND_TYPE>
  ERT_COMMAND_TYPE
  get_ert_cmd()
  {
    return reinterpret_cast<ERT_COMMAND_TYPE>(get_ert_packet());
  }

  /**
   * Add a callback, synchronize with concurrent state change
   * Call the callback if command is complete.
   */
  void
  add_callback(callback_function_type fcn)
  {
    bool complete = false;
    ert_cmd_state state;
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      if (!m_callbacks)
        m_callbacks = std::make_unique<callback_list>();
      m_callbacks->emplace_back(std::move(fcn));
      complete = m_done;
      auto pkt = get_ert_packet();
      state = static_cast<ert_cmd_state>(pkt->state);
      if (complete && state < ERT_CMD_STATE_COMPLETED)
        throw std::runtime_error("Unexpected state");
    }

    // lock must not be helt while calling callback function
    if (complete)
      m_callbacks.get()->back()(state);
  }

  /**
   * Run registered callbacks.
   */
  void
  run_callbacks(ert_cmd_state state) const
  {
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      if (!m_callbacks)
        return;
    }

    // cannot lock mutex while calling the callbacks
    // so copy address of callbacks while holding the lock
    // then execute callbacks without lock
    std::vector<callback_function_type*> copy;
    copy.reserve(m_callbacks->size());

    {
      std::lock_guard<std::mutex> lk(m_mutex);
      std::transform(m_callbacks->begin(),m_callbacks->end()
                     ,std::back_inserter(copy)
                     ,[](callback_function_type& cb) { return &cb; });
    }

    for (auto cb : copy)
      (*cb)(state);
  }

  /**
   * Submit the command for execution
   */
  void
  run()
  {
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      if (!m_done)
        throw std::runtime_error("bad command state, can't launch");
      m_done = false;
    }
    xrt_core::exec::schedule(this);
  }

  /**
   * Wait for command completion
   */
  ert_cmd_state
  wait() const
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (!m_done)
      m_exec_done.wait(lk);

    auto pkt = get_ert_packet();
    return static_cast<ert_cmd_state>(pkt->state);
  }

  ////////////////////////////////////////////////////////////////
  // Implement xrt_core::command API
  ////////////////////////////////////////////////////////////////
  virtual ert_packet*
  get_ert_packet() const
  {
    return reinterpret_cast<ert_packet*>(m_execbuf.second);
  }

  virtual xrt_core::device*
  get_device() const
  {
    return m_device->get_core_device();
  }

  virtual xclBufferHandle
  get_exec_bo() const
  {
    return m_execbuf.first;
  }

  virtual void
  notify(ert_cmd_state s)
  {
    bool complete = false;
    if (s>=ERT_CMD_STATE_COMPLETED) {
      std::lock_guard<std::mutex> lk(m_mutex);
      complete = m_done = true;
      m_exec_done.notify_all();  // CAN THIS BE MOVED TO END AFTER CALLBACKS?
    }

    if (complete)
      run_callbacks(s);
  }

private:
  device_type* m_device = nullptr;
  execbuf_type m_execbuf; // underlying execution buffer
  bool m_done = false;

  mutable std::mutex m_mutex;
  mutable std::condition_variable m_exec_done;

  std::unique_ptr<callback_list> m_callbacks;
};

// struct kernel_type - The internals of an xrtKernelHandle
//
// An single object of kernel_type can be shared with multiple
// run handles.   The kernel object defines all kernel specific
// meta data used to create a launch a run object (command)
struct kernel_type
{
  using argument = xrt_core::xclbin::kernel_argument;

  std::shared_ptr<device_type> device;   // shared ownership
  std::string name;                      // kernel name
  const axlf* top = nullptr;             // xclbin
  std::vector<const ip_data*> ips;       // compute units
  std::vector<argument> args;            // kernel args sorted by argument index
  std::bitset<128> cumask;               // cumask for command execution
  size_t regmap_size = 0;                // CU register map size
  size_t num_cumasks = 1;                // Required number of command cu masks TODO: compute

  // kernel_type - constructor
  //
  // @dev:     device associated with this kernel object
  // @xclbin:  xclbin to mine for kernel meta data
  // @nm:      name identifying kernel and/or kernel and instances
  kernel_type(std::shared_ptr<device_type> dev, const char* xclbin, const std::string& nm)
    : device(std::move(dev))                                   // share ownership
    , name(nm.substr(0,nm.find(":")))                          // filter instance names
    , top(reinterpret_cast<const axlf*>(xclbin))               // convert to axlf
    , ips(xrt_core::xclbin::get_cus(top, nm))                  // CUs matching nm
    , args(xrt_core::xclbin::get_kernel_arguments(top, name))  // kernel argument meta data
  {
    // Compare the matching CUs against the CU sort order to create cumask
    if (ips.empty())
      throw std::runtime_error("No compute units matching '" + nm + "'");
    auto cus = xrt_core::xclbin::get_cus(top);  // sort order
    for (const ip_data* cu : ips) {
      auto itr = std::find(cus.begin(), cus.end(), cu->m_base_address);
      if (itr == cus.end())
        throw std::runtime_error("unexpected error");
      cumask.set(std::distance(cus.begin(), itr));
    }

    // Compute register map size for a kernel invocation
    for (auto& arg : args)
      regmap_size = std::max(regmap_size, (arg.offset + arg.size) / 4);
  }
};

// struct run_type - The internals of an xrtRunHandle
//
// An run handle shares ownership of a kernel object.  The run object
// corresponds to an execution context for the given kernel object.
// Multiple run objects against the same kernel object can be created
// and submitted for execution concurrently.  Each run object manages
// its own execution buffer (ert command object)
struct run_type
{
  using callback_function_type = std::function<void(ert_cmd_state)>;
  std::shared_ptr<kernel_type> kernel;    // shared ownership
  xrt_core::device* core_device;          // convenience, in scope of kernel
  kernel_command cmd;                     // underlying command object

  void
  add_callback(callback_function_type fcn)
  {
    cmd.add_callback(fcn);
  }

  // run_type() - constructor
  //
  // @krnl:  kernel object to run
  run_type(std::shared_ptr<kernel_type> k)
    : kernel(std::move(k))                           // share ownership
    , core_device(kernel->device->get_core_device()) // cache core device
    , cmd(kernel->device.get())
  {
    // TODO: consider if execbuf is cleared on return from cache
    auto kcmd = cmd.get_ert_cmd<ert_start_kernel_cmd*>();
    kcmd->count = kernel->num_cumasks + kernel->regmap_size;
    kcmd->opcode = ERT_START_CU;
    kcmd->type = ERT_CU;
    kcmd->cu_mask = kernel->cumask.to_ulong();  // TODO: fix for > 32 CUs
  }

  // set_global_arg() - set a global argument
  void
  set_global_arg(size_t index, xrtBufferHandle bo)
  {
    xclBOProperties prop;
    core_device->get_bo_properties(bo, &prop);
    auto addr = prop.paddr;

    // Populate cmd payload with argument
    auto kcmd = cmd.get_ert_cmd<ert_start_kernel_cmd*>();
    const auto& arg = kernel->args[index];
    auto cmdidx = arg.offset / 4;
    kcmd->data[cmdidx] =  addr;
    kcmd->data[++cmdidx] = (addr >> 32) & 0xFFFFFFFF;
  }

  // set_scalar_arg() - set a scalar argument
  template <typename ScalarType>
  void
  set_scalar_arg(size_t index, ScalarType scalar)
  {
    static_assert(std::is_scalar<ScalarType>::value,"Invalid ScalarType");
    // Populate cmd payload with argument
    auto kcmd = cmd.get_ert_cmd<ert_start_kernel_cmd*>();
    const auto& arg = kernel->args[index];
    auto cmdidx = arg.offset / 4;
    kcmd->data[cmdidx] =  scalar;
  }

  void
  set_arg_at_index(size_t index, std::va_list args)
  {
    auto& arg = kernel->args[index];
    if (arg.index == kernel_type::argument::no_index)
      throw std::runtime_error("Bad argument index '" + std::to_string(index) + "'");

    switch (arg.type) {
    case kernel_type::argument::argtype::scalar : {
      auto val = va_arg(args, size_t); // TODO: handle double, and more get type from meta data
      set_scalar_arg(arg.index, val);
      break;
    }
    case kernel_type::argument::argtype::global : {
      auto val = va_arg(args, xrtBufferHandle);
      set_global_arg(arg.index, val);
      break;
    }
    default:
      throw std::runtime_error("Unexpected error");
    }
  }

  void
  set_all_args(std::va_list args)
  {
    for (auto& arg : kernel->args) {
      if (arg.index == kernel_type::argument::no_index)
        break;
      set_arg_at_index(arg.index, args);
    }
  }

  // start() - start the run object (execbuf)
  void
  start()
  {
    auto pkt = cmd.get_ert_packet();
    pkt->state = ERT_CMD_STATE_NEW;
    cmd.run();
  }

  // wait() - wait for execution to complete
  ert_cmd_state
  wait() const
  {
    return cmd.wait();
  }

  // state() - get current execution state
  ert_cmd_state
  state() const
  {
    auto pkt = cmd.get_ert_packet();
    return static_cast<ert_cmd_state>(pkt->state);
  }
};

// Device wrapper.  Lifetime is tied to kernel object.  Using
// std::weak_ptr to treat as cache rather sharing ownership.
// Ownership of device is shared by kernel objects, when last kernel
// object is destructed, the correponding device object is deleted and
// cache will miss lookup for subsequent kernel creation.  Without
// weak_ptr, the cache would hold on to the device until static global
// destruction and long after application calls xclClose on the
// xrtDeviceHandle.
static std::map<xrtDeviceHandle, std::weak_ptr<device_type>> devices;

// Active kernels per xrtKernelOpen/Close.  This is a mapping from
// xrtKernelHandle to the corresponding kernel object.  The
// xrtKernelHandle is the address of the kernel object.  This is
// shared ownership as application can close a kernel handle before
// closing an xrtRunHandle that references same kernel.
static std::map<void*, std::shared_ptr<kernel_type>> kernels;

// Active runs.  This is a mapping from xrtRunHandle to corresponding
// run object.  The xrtRunHandle is the address of the run object.
// This is unique ownership as only the host application holds on to a
// run object, e.g. the run object is desctructed immediately when it
// is closed.
static std::map<void*, std::unique_ptr<run_type>> runs;

// get_device() - get a device object from an xrtDeviceHandle
//
// The lifetime of the device object is shared ownership. The object
// is cached so that subsequent look-ups from same xrtDeviceHandle
// result in same device object if it exists already.
static std::shared_ptr<device_type>
get_device(xrtDeviceHandle dhdl, const char* xclbin)
{
  auto itr = devices.find(dhdl);
  std::shared_ptr<device_type> device = (itr != devices.end())
    ? (*itr).second.lock()
    : nullptr;
  if (!device) {
    device = std::shared_ptr<device_type>(new device_type(dhdl));
    xrt_core::exec::init(device->get_core_device(), reinterpret_cast<const axlf*>(xclbin));
    devices.emplace(std::make_pair(dhdl, device));
  }
  return device;
}

// get_kernel() - get a kernel object from an xrtKernelHandle
//
// The lifetime of a kernel object is shared ownerhip. The object
// is shared with host application and run objects.
static std::shared_ptr<kernel_type>
get_kernel(xrtKernelHandle khdl)
{
  auto itr = kernels.find(khdl);
  if (itr == kernels.end())
    throw std::runtime_error("Unknown kernel handle");
  return (*itr).second;
}

// get_run() - get a run object from an xrtRunHandle
//
// The lifetime of a run object is unique to the host application.
static run_type*
get_run(xrtRunHandle rhdl)
{
  auto itr = runs.find(rhdl);
  if (itr == runs.end())
    throw std::runtime_error("Unknown run handle");
  return (*itr).second.get();
}

namespace api {

xrtKernelHandle
xrtKernelOpen(xrtDeviceHandle dhdl, const char* xclbin, const char *name)
{
  auto device = get_device(dhdl, xclbin);
  auto kernel = std::make_shared<kernel_type>(device, xclbin, name);
  auto handle = kernel.get();
  kernels.emplace(std::make_pair(handle,std::move(kernel)));
  return handle;
}

void
xrtKernelClose(xrtKernelHandle khdl)
{
  auto itr = kernels.find(khdl);
  if (itr == kernels.end())
    throw std::runtime_error("Unknown kernel handle");
  kernels.erase(itr);
}

xrtRunHandle
xrtRunOpen(xrtKernelHandle khdl)
{
  auto kernel = get_kernel(khdl);
  auto run = std::make_unique<run_type>(kernel);
  auto handle = run.get();
  runs.emplace(std::make_pair(handle,std::move(run)));
  return handle;
}

void
xrtRunClose(xrtRunHandle rhdl)
{
  auto itr = runs.find(rhdl);
  if (itr == runs.end())
    throw std::runtime_error("Unknown run handle");
  runs.erase(itr);
}

ert_cmd_state
xrtRunState(xrtRunHandle rhdl)
{
  auto run = get_run(rhdl);
  return run->state();
}

ert_cmd_state
xrtRunWait(xrtRunHandle rhdl)
{
  auto run = get_run(rhdl);
  return run->wait();
}

void
xrtRunSetCallback(xrtRunHandle rhdl, ert_cmd_state state,
                  void (* pfn_state_notify)(xrtRunHandle, ert_cmd_state, void*),
                  void* data)
{
  if (state != ERT_CMD_STATE_COMPLETED)
    throw std::runtime_error("xrtRunSetCallback state may only be ERT_CMD_STATE_COMPLETED");
  auto run = get_run(rhdl);
  run->add_callback([=](ert_cmd_state state) { pfn_state_notify(rhdl, state, data); });
}

void
xrtRunStart(xrtRunHandle rhdl)
{
  auto run = get_run(rhdl);
  run->start();
}

} // api

inline void
send_exception_message(const char* msg)
{
  xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", msg);
}

} // namespace

////////////////////////////////////////////////////////////////
// xrt_kernel API implmentations (xrt_kernel.h)
////////////////////////////////////////////////////////////////
xrtKernelHandle
xrtKernelOpen(xrtDeviceHandle dhdl, const char* xclbin, const char *name)
{
  try {
    return api::xrtKernelOpen(dhdl, xclbin, name);
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

int
xrtKernelClose(xrtKernelHandle khdl)
{
  try {
    api::xrtKernelClose(khdl);
    return 0;
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

xrtRunHandle
xrtRunOpen(xrtKernelHandle khdl)
{
  try {
    return api::xrtRunOpen(khdl);
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

xrtRunHandle
xrtKernelRun(xrtKernelHandle khdl, ...)
{
  try {
    auto handle = xrtRunOpen(khdl);
    auto run = get_run(handle);
    auto kernel = run->kernel;

    std::va_list args;
    va_start(args, khdl);
    run->set_all_args(args);
    va_end(args);

    run->start();

    return handle;
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

int
xrtRunClose(xrtRunHandle rhdl)
{
  try {
    api::xrtRunClose(rhdl);
    return 0;
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

ert_cmd_state
xrtRunState(xrtRunHandle rhdl)
{
  try {
    return api::xrtRunState(rhdl);
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return ERT_CMD_STATE_ABORT;
}

ert_cmd_state
xrtRunWait(xrtRunHandle rhdl)
{
  try {
    return api::xrtRunWait(rhdl);
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return ERT_CMD_STATE_ABORT;
  }
}

int
xrtRunSetCallback(xrtRunHandle rhdl, ert_cmd_state state,
                  void (* pfn_state_notify)(xrtRunHandle, ert_cmd_state, void*),
                  void* data)
{
  try {
    api::xrtRunSetCallback(rhdl, state, pfn_state_notify, data);
    return 0;
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtRunStart(xrtRunHandle rhdl)
{
  try {
    api::xrtRunStart(rhdl);
    return 0;
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtRunSetArg(xrtRunHandle rhdl, int index, ...)
{
  try {
    auto run = get_run(rhdl);

    std::va_list args;
    va_start(args, index);
    run->set_arg_at_index(index, args);
    va_end(args);

    return 0;
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}
