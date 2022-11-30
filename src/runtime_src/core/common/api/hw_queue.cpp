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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include <iostream>

using namespace std::chrono_literals;

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

  // If retain is last reference to cmd, then the command object is
  // deleted here from the monitor thread, but that can result in
  // hwqueue destruction (command owns reference). If the hwqueue owns
  // the monitor thread and stops and joins with it, the result will
  // be a resource deadlock exception.
  cmd->notify(state);
}

inline void
notify_host(xrt_core::command* cmd)
{
  notify_host(cmd, get_command_state(cmd));
}

// class command_manager - managed command executuon
//
// @m_qimpl: The hw queue used for command submission
// @work_mutex: Syncrhonize monitor thread with launched commands
// @work_cond: Kick off monitor thread when there are new commands
// @monitor_thread: Thread for asynchronous monitoring of command execution
// @stop: Stop the monitor thread
//
// This is constructed on demand when commands are submitted for managed
// execution through a command queue.  Managed execution means that
// commands are submitted for execution and receive a callback on
// completion.  This is the OpenCL model but is also supported by
// native XRT APIs.
//
// The command manager requires submission and wait APIs to be implemented
// by which ever object (hw queue) uses the manager.
class command_manager
{
public:
  struct executor
  {
    virtual std::cv_status
    wait(size_t timeout_ms) = 0;

    virtual void
    submit(xrt_core::command* cmd) = 0;
  };

private:
  executor* m_impl;
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
      m_impl->wait(0);

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
  command_manager(executor* impl)
    : m_impl(impl), monitor_thread(xrt_core::thread(&command_manager::monitor, this))
  {
    XRT_DEBUGF("command_manager::command_manager(0x%x)\n", impl);
  }

  // Destructor stops and joins monitor thread
  ~command_manager()
  {
    XRT_DEBUGF("command_manager::~command_manager() executor(0x%x)\n", m_impl);
    stop = true;
    work_cond.notify_one();
    monitor_thread.join();
  }

  void
  clear_executor()
  {
    m_impl = nullptr;
  }

  void
  set_executor(executor* impl)
  {
    m_impl = impl;
  }

  // launch() - Submit a command for managed execution
  //
  // This function is used to schedule managed commands for
  // execution. Managed means that the command will be monitored for
  // completion and notified upon completion.  Notification is through
  // command callback.
  void
  launch(xrt_core::command* cmd)
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
      m_impl->submit(cmd);
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
};

// Ideally a command manager should be owned by a hw_queue which
// constructs the manager on demand.  But there is a thread exit
// problem that can result in resource deadlock exception when the
// hwqueue is destructed from the monitor thread itself as part of
// calling notify_host (see comment in notify_host() above).
//
// To work-around this, command_managers are managed globally and
// destructed only at program exit by the main thread.  The managers
// are recycled for subsequent use by a new hwqueue.
//
// Statically allocated command managers to handle thread exit
static std::vector<std::unique_ptr<command_manager>> s_command_manager_pool;
static std::mutex s_pool_mutex;

} // namespace

namespace xrt_core {

// class hw_queue_impl - base implementation class
//
// Implements the interface required for both managed
// and unmanaged execution.
class hw_queue_impl : public command_manager::executor
{
  std::unique_ptr<command_manager> m_cmd_manager;
  unsigned int m_uid = 0;

  // Thread safe on-demand creation of m_cmd_manager
  command_manager*
  get_cmd_manager()
  {
    std::lock_guard lk(s_pool_mutex);

    if (m_cmd_manager)
      return m_cmd_manager.get();

    // Use recycled manager if any
    if (!s_command_manager_pool.empty()) {
      m_cmd_manager = std::move(s_command_manager_pool.back());
      s_command_manager_pool.pop_back();
      m_cmd_manager->set_executor(this);
      return m_cmd_manager.get();
    }

    // Construct new manager
    m_cmd_manager = std::make_unique<command_manager>(this);
    return m_cmd_manager.get();
  }

public:
  hw_queue_impl()
  {
    static unsigned int count = 0;
    m_uid = count++;
    XRT_DEBUGF("hw_queue_impl::hw_queue_impl(%d) this(0x%x)\n", m_uid, this);
  }

  virtual
  ~hw_queue_impl()
  {
    XRT_DEBUGF("hw_queue_impl::~hw_queue_impl(%d)\n", m_uid);
    if (m_cmd_manager) {
      m_cmd_manager->clear_executor();
      std::lock_guard lk(s_pool_mutex);
      s_command_manager_pool.push_back(std::move(m_cmd_manager));
    }
  }

  // Submit command for execution
  virtual void
  submit(xrt_core::command* cmd) = 0;

  // Wait for some command to finish
  virtual std::cv_status
  wait(size_t timeout_ms) = 0;

  // Wait for specified command to finish
  virtual std::cv_status
  wait(const xrt_core::command* cmd, size_t timeout_ms) = 0;

  // Managed start uses command manager for monitoring command
  // completion
  void
  managed_start(xrt_core::command* cmd)
  {
    get_cmd_manager()->launch(cmd);
  }

  // Unmanaged start submits command directly for execution
  // Command completion must be explicitly managed by application
  void
  unmanaged_start(xrt_core::command* cmd)
  {
    submit(cmd);
  }
};

// class qds_device - queue implementation for shim queue support
class qds_device : public hw_queue_impl
{
  xrt_core::device* m_device;
  xcl_hwqueue_handle m_qhdl;

public:
  qds_device(xrt_core::device* device, xcl_hwqueue_handle qhdl)
    : m_device(device), m_qhdl(qhdl)
  {}

  ~qds_device()
  {
    m_device->destroy_hw_queue(m_qhdl);
  }

  std::cv_status
  wait(size_t timeout_ms) override
  {
    return m_device->wait_command(m_qhdl, XRT_INVALID_BUFFER_HANDLE, static_cast<int>(timeout_ms))
      ? std::cv_status::no_timeout
      : std::cv_status::timeout;
  }

  std::cv_status
  wait(const xrt_core::command* cmd, size_t timeout_ms) override
  {
    auto pkt = cmd->get_ert_packet();
    while (pkt->state < ERT_CMD_STATE_COMPLETED) {
      // return immediately on timeout
      if (m_device->wait_command(m_qhdl, cmd->get_exec_bo(), static_cast<int>(timeout_ms)) == 0)
        return std::cv_status::timeout;
    }

    // notify_host is not strictly necessary for unmanaged
    // command execution but provides a central place to update
    // and mark commands as done so they can be re-executed.
    notify_host(const_cast<xrt_core::command*>(cmd), static_cast<ert_cmd_state>(pkt->state));

    return std::cv_status::no_timeout;
  }

  void
  submit(xrt_core::command* cmd) override
  {
    m_device->submit_command(m_qhdl, cmd->get_exec_bo());
  }
};

// class kds_device - queue implementation for legacy shim support
//
// @exec_wait_mutex: Synchronize access to exec_wait
// @exec_wait_call_count:  Count of number of calls to exec wait
class kds_device : public hw_queue_impl
{
  xrt_core::device* m_device;
  std::mutex m_exec_wait_mutex;
  std::condition_variable m_work;
  uint64_t m_exec_wait_call_count {0};
  uint32_t m_exec_wait_active {0};

  // Thread safe shim level exec wait call.   This function allows
  // multiple threads to call exec_wait through same device handle.
  //
  // In multi-threaded applications it is possible that a call to shim
  // level exec_wait by one thread covers completion for other
  // threads.  Without careful synchronization, a thread that calls
  // device::exec_wait could end up being stuck either forever or
  // until some other unrelated command completes.  This function
  // prevents that scenario from happening.
  //
  // Thread local storage keeps a call count that syncs with the
  // number of times device::exec_wait has been called. If thread
  // local call count is different from the global count, then this
  // function resets the thread local call count and return without
  // calling device::exec_wait.
  //
  // In order to reduce multi-threaded wait time, condition variable
  // wait is used for subsequent threads calling this function while
  // some other thread is busy running device->exec_wait. Condition
  // variable wait and wake up is faster than letting multiple threads
  // wait on a single mutex lock.  This means that device::exec_wait
  // is done by the first host thread that needs the call to
  // exec_wait, while condition variable notification is used for
  // additional threads.
  //
  // The specified timeout affects the waiting for device::exec_wait
  // only. The timeout can be masked if device is busy and many
  // commands complete with the specified timeout.
  std::cv_status
  exec_wait(size_t timeout_ms=0)
  {
    static thread_local uint64_t thread_exec_wait_call_count = 0;

    // Critical section to check if this thread needs to call
    // device::exec_wait or should wait on some other thread
    // completing the call.
    {
      std::unique_lock lk(m_exec_wait_mutex);
      if (thread_exec_wait_call_count != m_exec_wait_call_count) {
        // Some other thread has called exec_wait and may have
        // covered this thread's commands, synchronize thread
        // local call count and return to caller.
        thread_exec_wait_call_count = m_exec_wait_call_count;
        return std::cv_status::no_timeout;
      }

      if (m_exec_wait_active) {
        // Some other thread is calling device::exec_wait, wait
        // for it complete its work and notify this thread
        auto status = std::cv_status::no_timeout;
        if (timeout_ms)
          status = (m_work.wait_for(lk, timeout_ms * 1ms,
                                    [this] {
                                      return thread_exec_wait_call_count != m_exec_wait_call_count;
                                    }))
            ? std::cv_status::no_timeout
            : std::cv_status::timeout;
        else
          m_work.wait(lk,
                      [this] {
                        return thread_exec_wait_call_count != m_exec_wait_call_count;
                      });

        // The other thread has completed its exec_wait call,
        // sync with current global call count and return
        thread_exec_wait_call_count = m_exec_wait_call_count;
        return status;
      }

      // Critical section updates wait status to prevent other threads
      // from calling exec_wait while this thread is calling it.
      ++m_exec_wait_active;
    }

    // Call device exec_wait without keeping the lock to allow other
    // threads to proceed into a conditional wait.  This scales better
    // than one giant exclusive region.  It is guaranteed that only
    // this thread will be here because other threads are blocked by
    // the active count that is only modified in the exclusive region.
    // assert(m_exec_wait_active == 1);
    auto status = std::cv_status::no_timeout;
    if (timeout_ms) {
      // device exec_wait is a system poll which returns
      // 0 when specified timeout is exceeded without any
      // file descriptors to read
      if (m_device->exec_wait(static_cast<int>(timeout_ms)) == 0)
        // nothing happened within specified time
        status = std::cv_status::timeout;
    }
    else {
      // wait for ever for some command to complete
      while (m_device->exec_wait(1000) == 0) {}
    }

    // Acquire lock before updating shared state
    {
      std::lock_guard lk(m_exec_wait_mutex);
      thread_exec_wait_call_count = ++m_exec_wait_call_count;
      --m_exec_wait_active;
    }

    // Notify any waiting threads so they can check command status and
    // possibly call exec_wait again.
    m_work.notify_all();

    return status;
  }

public:
  kds_device(xrt_core::device* device)
    : m_device(device)
  {}

  std::cv_status
  wait(size_t timeout_ms) override
  {
    return exec_wait(timeout_ms);
  }

  std::cv_status
  wait(const xrt_core::command* cmd, size_t timeout_ms) override
  {
    volatile auto pkt = cmd->get_ert_packet();
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

  void
  submit(xrt_core::command* cmd) override
  {
    m_device->exec_buf(cmd->get_exec_bo());
  }
};

}  // xrt_core

namespace {

// Manage hw_queue implementations.
// For time being there is only one hw_queue per hw_context
// Use static map with weak pointers to implementation.
using hwc2hwq_type = std::map<xcl_hwctx_handle, std::weak_ptr<xrt_core::hw_queue_impl>>;
using queue_ptr = std::shared_ptr<xrt_core::hw_queue_impl>;
static std::mutex mutex;
static std::map<const xrt_core::device*, hwc2hwq_type> dev2hwc;  // per device

// This function ensures that only one kds_device is created per
// xrt_core::device regardless of hwctx.  It allocates (if necessary)
// a kds_device queue impl in the sentinel slot that represents a null
// hardware context.
static std::shared_ptr<xrt_core::hw_queue_impl>
get_kds_device_nolock(hwc2hwq_type& queues, const xrt_core::device* device)
{
  auto hwqimpl = queues[XRT_NULL_HWCTX].lock();
  if (!hwqimpl)
    queues[XRT_NULL_HWCTX] = hwqimpl =
      queue_ptr{new xrt_core::kds_device(const_cast<xrt_core::device*>(device))};

  return hwqimpl;
}

// Create a hw_queue implementation assosicated with a device without
// any hw context.  This is used for legacy construction for internal
// support of command execution that is not tied to kernel execution,
// .e.g for copy_bo_with_kdma.
static std::shared_ptr<xrt_core::hw_queue_impl>
get_hw_queue_impl(const xrt_core::device* device)
{
  std::lock_guard lk(mutex);
  auto& queues = dev2hwc[device];
  return get_kds_device_nolock(queues, device);
}

// Create a hw_queue implementation for a hw context.
// Ensure unique queue per device since driver doesn't currently
// guarantee unique hwctx handle cross devices.  Also make sure
// only one kds_device queue impl is created per device.
static std::shared_ptr<xrt_core::hw_queue_impl>
get_hw_queue_impl(const xrt::hw_context& hwctx)
{
  auto device = xrt_core::hw_context_int::get_core_device_raw(hwctx);
  auto hwctx_hdl = static_cast<xcl_hwctx_handle>(hwctx);
  std::lock_guard lk(mutex);
  auto& queues = dev2hwc[device];
  auto hwqimpl = queues[hwctx_hdl].lock();
  if (!hwqimpl) {
    auto hwqueue_hdl = device->create_hw_queue(hwctx);
    queues[hwctx_hdl] = hwqimpl = (hwqueue_hdl == XRT_NULL_HWQUEUE)
      ? get_kds_device_nolock(queues, device)
      : queue_ptr{new xrt_core::qds_device(device, hwqueue_hdl)};
  }
  return hwqimpl;
}

}

namespace xrt_core {

////////////////////////////////////////////////////////////////
// Public APIs
////////////////////////////////////////////////////////////////
hw_queue::
hw_queue(const xrt::hw_context& hwctx)
  : xrt::detail::pimpl<hw_queue_impl>(get_hw_queue_impl(hwctx))
{}

hw_queue::
hw_queue(const xrt_core::device* device)
  : xrt::detail::pimpl<hw_queue_impl>(get_hw_queue_impl(device))
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
  get_handle()->wait(cmd, 0);
}

// Wait for command completion for unmanaged command execution with timeout
std::cv_status
hw_queue::
wait(const xrt_core::command* cmd, const std::chrono::milliseconds& timeout_ms) const
{
  return get_handle()->wait(cmd, timeout_ms.count());
}

std::cv_status
hw_queue::
exec_wait(const xrt_core::device* device, const std::chrono::milliseconds& timeout_ms)
{
  // bypass hwqueue and call exec wait directly
  auto impl = get_hw_queue_impl(device);

  // add dummy hwqueue null arg when changing shim
  return impl->wait(timeout_ms.count());
}

void
hw_queue::
stop()
{
  // Ensure all threads are joined prior to other cleanup
  // This is used by OpenCL code path before it deletes the
  // global platform and takes care of completing outstanding
  // event synchronization for commands.
  std::lock_guard lk(s_pool_mutex);
  s_command_manager_pool.clear();
}

} // namespace xrt_core
