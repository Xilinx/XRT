// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#define XRT_API_SOURCE         // in same dll as API sources

#include "hw_queue.h"

#include "command.h"
#include "hw_context_int.h"

#include "core/common/debug.h"
#include "core/common/device.h"
#include "core/common/thread.h"
#include "experimental/xrt_hw_context.h"
#include "core/include/ert.h"
#include "core/include/xcl_hwqueue.h"

#include <algorithm>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

////////////////////////////////////////////////////////////////
// Main command execution interface for scheduling commands for
// execution and waiting for commands to complete via KDS.
// This code is lifted from original kds.cpp and exec.{h,cpp}.
// There is no longer a need to support software scheduling since
// all shims support kds (exec_buf and exec_wait).
////////////////////////////////////////////////////////////////
namespace {

using command_queue_type = std::vector<xrt_core::command*>;
static std::exception_ptr s_exception;

inline ert_cmd_state
get_command_state(xrt_core::command* cmd)
{
  auto epacket = cmd->get_ert_packet();
  return static_cast<ert_cmd_state>(epacket->state);
}

inline bool
completed(xrt_core::command* cmd)
{
  return (get_command_state(cmd) >= ERT_CMD_STATE_COMPLETED);
}

inline void
notify_host(xrt_core::command* cmd, ert_cmd_state state)
{
  XRT_DEBUGF("xrt_core::kds::command(%d), [running->done]\n", cmd->get_uid());
  auto retain = cmd->shared_from_this();
  cmd->notify(state);
}

inline void
notify_host(xrt_core::command* cmd)
{
  notify_host(cmd, get_command_state(cmd));
}


// class kds_device - kds book keeping data for command scheduling
//
// @device: The core device used for shim level calls
// @exec_wait_mutex: Synchronize acces to exec_wait
// @work_mutex: Syncrhonize monitor thread with launched commands
// @work_cond: Kick off monitor thread when there are new commands
// @monitor_thread: Thread for asynchronous monitoring of command execution
// @exec_wait_call_count:  Count of number of calls to exec wait
// @stop: Stop the monitor thread
//
// This class is per xrt_core::device. The class constructor starts a
// command monitor thread that manages command execution.  It also
// provides a thread safe interface to shim level exec_wait which can
// be called explicitly to wait for command completion.
class kds_device
{
  xrt_core::device* device;
  std::mutex exec_wait_mutex;
  std::mutex work_mutex;
  std::condition_variable work_cond;
  command_queue_type submitted_cmds;
  uint64_t exec_wait_call_count = 0;
  bool stop = false;

  // thread can be constructed only after data members are initialized
  std::thread monitor_thread;

  // monitor_loop() - Manage running commands and notify on completion
  //
  // The monitor thread services managed command and asynchronously
  // notifies commands that are found to have completed.
  //
  // Commands that are submitted for execution using managed_start()
  // are monitored for completion by this function.
  void
  monitor_loop()
  {
    std::vector<xrt_core::command*> busy_cmds;
    std::vector<xrt_core::command*> running_cmds;

    while (1) {

      // Larger wait synchronized with launch()
      {
        std::unique_lock<std::mutex> lk(work_mutex);
        while (!stop && running_cmds.empty() && submitted_cmds.empty())
          work_cond.wait(lk);
      }

      if (stop)
        return;

      // Finer wait
      exec_wait();

      // Drain submitted commands.  It is important that this comes
      // after exec_wait and is synchronized with launch() that added
      // to submitted_cmds.
      //
      // Scenario if before exec_wait is that a new command was added
      // to submitted_cmds and exec_buf immediately after the critical
      // section above and that the command completion happens in the
      // exec_wait call. If submitted_cmds was drained, in for example
      // above critical section, before the call to exec_wait it would
      // not be in running_cmds and would not be notified of
      // completion.
      //
      // The sequence is very important.  It must be guaranteed that
      // exec_wait will never return for a command that is not yet
      // in either running_cmds or submitted_cmds.
      {
        std::lock_guard<std::mutex> lk(work_mutex);
        std::copy(submitted_cmds.begin(), submitted_cmds.end(), std::back_inserter(running_cmds));
        submitted_cmds.clear();
      }
      // At this point running_cmds is guaranteed to contain the
      // command(s) for which exec_wait returned.

      // Preserve order of processing
      for (auto cmd : running_cmds) {
        if (completed(cmd))
          notify_host(cmd);
        else
          busy_cmds.push_back(cmd);
      }

      running_cmds.swap(busy_cmds);
      busy_cmds.clear();
    } // while (1)
  }

  // Start the monitor thread
  void
  monitor()
  {
    try {
      monitor_loop();
    }
    catch (const std::exception& ex) {
      std::string msg = std::string("kds command monitor died unexpectedly: ") + ex.what();
      xrt_core::send_exception_message(msg.c_str());
      s_exception = std::current_exception();
    }
    catch (...) {
      xrt_core::send_exception_message("kds command monitor died unexpectedly");
      s_exception = std::current_exception();
    }
  }


public:
  // Constructor starts monitor thread
  kds_device(xrt_core::device* dev)
      : device(dev), monitor_thread(xrt_core::thread(&kds_device::monitor, this))
  {}

  // Destructor stops and joins monitor thread
  ~kds_device()
  {
    stop = true;
    work_cond.notify_one();
    monitor_thread.join();
  }

  // Thread safe shim level exec wait call.   This function allows
  // multiple threads to call exec_wait through same device handle.
  //
  // In multi-threaded applications it is possible that a call to shim
  // level exec_wait by one thread covers completion for other
  // threads.  Without careful synchronization, a thread that calls
  // device->exec_wait could end up being stuck either forever or
  // until some other unrelated command completes.  This function
  // prevents that scenario from happening.
  //
  // Thread local storage keeps a call count that syncs with the
  // number of times device->exec_wait has been called. If thread
  // local call count is different from the global count, then this
  // function resets the thread local call count and return without
  // calling shim exec_wait.
  //
  // The specified timeout has effect only when underlying xclExecWait
  // times out. The timeout can be masked if device is busy and many
  // commands complete with the specified timeout.
  std::cv_status
  exec_wait(size_t timeout_ms=0)
  {
    static thread_local uint64_t thread_exec_wait_call_count = 0;
    std::lock_guard<std::mutex> lk(exec_wait_mutex);

    if (thread_exec_wait_call_count != exec_wait_call_count) {
      // some other thread has called exec_wait and may have
      // covered this thread's commands, synchronize thread
      // local call count and return to caller.
      thread_exec_wait_call_count = exec_wait_call_count;
      return std::cv_status::no_timeout;
    }

    auto status = std::cv_status::no_timeout;
    if (timeout_ms) {
      // device exec_wait is a system poll which returns
      // 0 when specified timeout is exceeded without any
      // file descriptors to read
      if (device->exec_wait(static_cast<int>(timeout_ms)) == 0)
        status = std::cv_status::timeout;
    }
    else {
      // wait for ever for some command to complete
      while (device->exec_wait(1000) == 0) {}
    }

    // synchronize this thread with total call count
    thread_exec_wait_call_count = ++exec_wait_call_count;

    return status;
  }

  // exec_wait() - Wait for specific command completion with optional timeout
  //
  // Wait for command completion
  // This function is safe to call for managed and unmanaged commands.
  std::cv_status
  exec_wait(const xrt_core::command* cmd, size_t timeout_ms=0)
  {
    auto pkt = cmd->get_ert_packet();
    while (pkt->state < ERT_CMD_STATE_COMPLETED) {
      // return immediately on timeout
      if (exec_wait(timeout_ms) == std::cv_status::timeout)
        return std::cv_status::timeout;
    }

    // notify_host is not strictly necessary for unmanaged
    // command execution but provides a central place to update
    // and mark commands as done so they can be re-executed.
    notify_host(const_cast<xrt_core::command*>(cmd), static_cast<ert_cmd_state>(pkt->state));

    return std::cv_status::no_timeout;
  }

  // exec_buf() - Submit a command for execution
  //
  // This function is used to schedule unmanaged commands for
  // execution. The execution monitor is by-passed and will be unaware
  // of argument command having been scheduled for execution.
  void
  exec_buf(xrt_core::command* cmd, const xrt::hw_context& hwctx)
  {    
    device->exec_buf_ctx(cmd->get_exec_bo(), hwctx);
  }

  // launch() - Submit a command for managed execution
  //
  // This function is used to schedule managed commands for
  // execution. Managed means that the command will be monitored for
  // completion and notified upon completion.  Notification is through
  // command callback.
  void
  launch(xrt_core::command* cmd, const xrt::hw_context& hwctx)
  {
    XRT_DEBUGF("xrt_core::kds::command(%d) [new->submitted->running]\n", cmd->get_uid());

    // Store command so completion can be tracked.  Make sure this is
    // done prior to exec_buf as exec_wait can otherwise be missed.
    // See detailed explanation in monitor loop.
    {
      std::lock_guard<std::mutex> lk(work_mutex);
      submitted_cmds.push_back(cmd);
    }

    // Submit the command
    try {
      exec_buf(cmd, hwctx);
    }
    catch (...) {
      // Remove the pending command
      std::lock_guard<std::mutex> lk(work_mutex);
      assert(get_command_state(cmd)==ERT_CMD_STATE_NEW);
      if (!submitted_cmds.empty())
        submitted_cmds.pop_back();
      throw;
    }

    // This is somewhat expensive, it is better to have this after the
    // exec_buf call so that actual execution doesn't have to wait.
    work_cond.notify_one();
  }
}; // kds_device

// Statically allocated kds_device object for each core device
static std::map<const xrt_core::device*, std::unique_ptr<kds_device>> kds_devices;

// Get or create kds_device object from core device
static kds_device*
get_kds_device(xrt_core::device* device)
{
  static std::mutex mutex;
  std::lock_guard lk(mutex);

  auto itr = kds_devices.find(device);
  if (itr != kds_devices.end())
    return (*itr).second.get();

  auto iitr = kds_devices.insert(std::make_pair(device, std::make_unique<kds_device>(device)));
  return (iitr.first)->second.get();
}

} // namespace

namespace xrt_core {

// class hw_queue_impl - implementation of hardware queue
//
// This class provides a command submission API on top of
// xrt_core::device::ishim APIs. It leverage the KDS execbuf
// and execwait protocol through kds_device.
class hw_queue_impl
{
  xrt::hw_context m_hwctx;
  device* m_core_device;
  xcl_hwqueue_handle m_hdl;

  // This is logically const
  mutable kds_device* m_kds_device;

public:
  // Construct from hardware context
  hw_queue_impl(xrt::hw_context hwctx)
    : m_hwctx(std::move(hwctx))
    , m_core_device(hw_context_int::get_core_device_raw(m_hwctx))
    , m_hdl(m_core_device->create_hw_queue(m_hwctx))
    , m_kds_device(get_kds_device(m_core_device))
  {}

  // This constructor is for legacy command execution where there
  // is no associated hw context.
  hw_queue_impl(xrt_core::device* device)
    : m_core_device(device)
    , m_hdl(XRT_NULL_HWQUEUE)
    , m_kds_device(get_kds_device(m_core_device))
  {}

  ~hw_queue_impl()
  {
    m_core_device->destroy_hw_queue(m_hdl);
  }

  // Managed start uses execution monitor for command completion
  void
  managed_start(xrt_core::command* cmd)
  {
    m_kds_device->launch(cmd, m_hwctx);
  }

  // Unmanaged start submits command directly for execution
  // Command completion must be explicitly managed by application
  void
  unmanaged_start(xrt_core::command* cmd)
  {
    m_kds_device->exec_buf(cmd, m_hwctx);
  }

  // Wait for command completion. Supports both managed and
  // unmanaged commands
  void
  wait(const xrt_core::command* cmd) const
  {
    m_kds_device->exec_wait(cmd);
  }

  // Wait for command completion with timeout. Supports both managed
  // and unmanaged commands
  std::cv_status
  wait(const xrt_core::command* cmd, const std::chrono::milliseconds& timeout_ms) const
  {
    return m_kds_device->exec_wait(cmd, timeout_ms.count());
  }
};

// For time being there is only one hw_queue per hw_context
// Use static map with weak pointers to implementation.
// Ensure unique queue per device since driver doesn't currently
// guarantee unique hwctx handle cross devices
static std::shared_ptr<hw_queue_impl>
get_hw_queue_impl(const xrt::hw_context& hwctx)
{
  using hwc2hwq_type = std::map<xcl_hwctx_handle, std::weak_ptr<hw_queue_impl>>;
  static std::mutex mutex;
  static std::map<device*, hwc2hwq_type> dev2hwc;  // per device
  auto device = xrt_core::hw_context_int::get_core_device_raw(hwctx);
  auto xhdl = static_cast<xcl_hwctx_handle>(hwctx);
  std::lock_guard lk(mutex);
  auto& queues = dev2hwc[device];
  auto hwqimpl = queues[xhdl].lock();
  if (!hwqimpl)
    queues[xhdl] = hwqimpl = std::shared_ptr<hw_queue_impl>(new hw_queue_impl(hwctx));

  return hwqimpl;
}

////////////////////////////////////////////////////////////////
// Public APIs
////////////////////////////////////////////////////////////////
hw_queue::
hw_queue(const xrt::hw_context& hwctx)
  : xrt::detail::pimpl<hw_queue_impl>(get_hw_queue_impl(hwctx))
{}

hw_queue::
hw_queue(const xrt_core::device* device)
  : xrt::detail::pimpl<hw_queue_impl>(std::make_shared<hw_queue_impl>(const_cast<xrt_core::device*>(device)))
{}

void
hw_queue::
managed_start(xrt_core::command* cmd)
{
  get_handle()->managed_start(cmd);
}

void
hw_queue::
unmanaged_start(xrt_core::command* cmd)
{
  get_handle()->unmanaged_start(cmd);
}

// Wait for command completion for unmanaged command execution
void
hw_queue::
wait(const xrt_core::command* cmd) const
{
  get_handle()->wait(cmd);
}

// Wait for command completion for unmanaged command execution with timeout
std::cv_status
hw_queue::
wait(const xrt_core::command* cmd, const std::chrono::milliseconds& timeout_ms) const
{
  return get_handle()->wait(cmd, timeout_ms);
}

std::cv_status
hw_queue::
exec_wait(const xrt_core::device* device, const std::chrono::milliseconds& timeout_ms)
{
  // bypass hwqueue and call exec wait directly
  auto kds_device = get_kds_device(const_cast<xrt_core::device*>(device));

  // add dummy hwqueue null arg when changing shim
  return kds_device->exec_wait(timeout_ms.count());
}

void
hw_queue::
stop()
{
  kds_devices.clear();
}

} // namespace xrt_core
