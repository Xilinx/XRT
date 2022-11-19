// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common

#include "exec.h"
#include "ert.h"
#include "command.h"
#include "core/common/debug.h"
#include "core/common/device.h"
#include "core/common/thread.h"

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

////////////////////////////////////////////////////////////////
// Main command execution interface for scheduling commands for
// execution and waiting for commands to complete.
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
    XRT_DEBUGF("command_manager::command_manager() this(0x%x) executor(0x%x)\n", this, impl);
  }

  // Destructor stops and joins monitor thread
  ~command_manager()
  {
    XRT_DEBUGF("command_manager::~command_manager() this(0x%x) executor(0x%x)\n", this, m_impl);
    {
      // Modify stop while keeping the lock so that the multi
      // conditional wait in monitor_loop is atomic.  E.g. a
      // std::atomic stop is not sufficient.
      std::lock_guard lk(work_mutex);
      stop = true;
      work_cond.notify_one();
    }
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

// At program exit, the command manager threads (monitor threads) must
// be stopped and joined.  Normally this is done during static global
// destruction, but in the OpenCL case a 'bad' program can exit before
// all monitor threads have completed their work.  This function
// handles that case and is used only by OpenCL.
static void
stop_monitor_threads()
{
    std::lock_guard lk(s_pool_mutex);
    XRT_DEBUGF("stop_monitor_threads() pool(%d)\n", s_command_manager_pool.size());
    s_command_manager_pool.clear();
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
class kds_device : public command_manager::executor
{
  xrt_core::device* m_device;
  std::unique_ptr<command_manager> m_cmd_manager;

  std::mutex m_exec_wait_mutex;
  std::condition_variable m_work;
  uint64_t m_exec_wait_call_count {0};

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
  // Constructor starts monitor thread
  kds_device(xrt_core::device* dev)
    : m_device(dev)
  {
    XRT_DEBUGF("xrt_core::kds_device::kds_device() this(0x%d)\n", this);
  }

  // Destructor stops and joins monitor thread
  ~kds_device()
  {
    XRT_DEBUGF("xrt_core::kds_device::~kds_device() this(0x%x)\n", this);
    if (m_cmd_manager) {
      m_cmd_manager->clear_executor();
      std::lock_guard lk(s_pool_mutex);
      s_command_manager_pool.push_back(std::move(m_cmd_manager));
    }
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
    std::lock_guard lk(m_exec_wait_mutex);

    if (thread_exec_wait_call_count != m_exec_wait_call_count) {
      // some other thread has called exec_wait and may have
      // covered this thread's commands, synchronize thread
      // local call count and return to caller.
      thread_exec_wait_call_count = m_exec_wait_call_count;
      return std::cv_status::no_timeout;
    }

    auto status = std::cv_status::no_timeout;
    if (timeout_ms) {
      // device exec_wait is a system poll which returns
      // 0 when specified timeout is exceeded without any
      // file descriptors to read
      if (m_device->exec_wait(static_cast<int>(timeout_ms)) == 0)
        status = std::cv_status::timeout;
    }
    else {
      // wait for ever for some command to complete
      while (m_device->exec_wait(1000) == 0) {}
    }

    // synchronize this thread with total call count
    thread_exec_wait_call_count = ++m_exec_wait_call_count;

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
  exec_buf(xrt_core::command* cmd)
  {
    m_device->exec_buf(cmd->get_exec_bo());
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
    get_cmd_manager()->launch(cmd);
  }

  ////////////////////////////////////////////////////////////////
  // command_manager::executor implementation
  ////////////////////////////////////////////////////////////////
  void
  submit(xrt_core::command* cmd) override
  {
    exec_buf(cmd);
  }

  std::cv_status
  wait(size_t timeout_ms) override
  {
    return exec_wait(timeout_ms);
  }

}; // kds_device

// Statically allocated kds_device object for each core deviced
static std::map<const xrt_core::device*, std::unique_ptr<kds_device>> kds_devices;
static std::mutex kds_devices_mutex;
static std::condition_variable device_erased;

// Get or create kds_device object from core device
static kds_device*
get_kds_device(xrt_core::device* device)
{
  std::lock_guard lk(kds_devices_mutex);
  auto itr = kds_devices.find(device);
  if (itr != kds_devices.end())
    return (*itr).second.get();

  auto iitr = kds_devices.insert(std::make_pair(device, std::make_unique<kds_device>(device)));
  return (iitr.first)->second.get();
}

// Get or existing kds_device object from core device.  Throw if
// kds_device object does not exist (internal error).
static kds_device*
get_kds_device_or_error(const xrt_core::device* device)
{
  std::lock_guard lk(kds_devices_mutex);
  auto itr = kds_devices.find(device);
  if (itr == kds_devices.end())
    throw std::runtime_error("internal error: missing kds device");
  return (*itr).second.get();
}

// Get kds_device from command object.  Throws if kds_device
// doesn't exist
static kds_device*
get_kds_device(const xrt_core::command* cmd)
{
  return get_kds_device_or_error(cmd->get_device());
}

// This function removes a device entry from the static map managed in
// this compilation unit.  While it is not wrong to keep stale device
// pointers in the map, they can accumulate and burn 8 bytes of memory
// if application constructs device after device. This function is
// called when an xrt_core::device is destructed.
static void
remove_device(const xrt_core::device* device)
{
  std::lock_guard lk(kds_devices_mutex);
  XRT_DEBUGF("remove_device(0x%x) kds_devices(%d)\n", device, kds_devices.size());
  kds_devices.erase(device);
  device_erased.notify_all();
}

// This function is used exclusively by OpenCL prior to stopping
// command manager monitor threads.  It is possible that during exit
// of an OpenCL application (see also stop_monitor_threads()) the
// devices are still busy executing commands.  This function waits for
// devices to be removed properly, or after a timeout, it simply uses
// the big hammer to cleanup, which implies that the OpenCL application
// did not properly clean up its resources.
static void
wait_while_devices()
{
  std::unique_lock lk(kds_devices_mutex);
  XRT_DEBUGF("wait_while_devices() wait for %d devices to clear\n", kds_devices.size());
  if (!device_erased.wait_for(lk, 200ms, [] { return kds_devices.empty(); }))
    // timeout, force stop the devices if application didn't free all
    // resources and relies on static destr where order is undefined.
    kds_devices.clear();
}

} // namespace


namespace xrt_core { namespace kds {

// Wait for command completion for unmanaged command execution
void
unmanaged_wait(const xrt_core::command* cmd)
{
  auto kdev = get_kds_device(cmd);
  kdev->exec_wait(cmd);
}

// Wait for command completion for unmanaged command execution with timeout
std::cv_status
unmanaged_wait(const xrt_core::command* cmd, const std::chrono::milliseconds& timeout_ms)
{
  auto kdev = get_kds_device(cmd);
  return kdev->exec_wait(cmd, timeout_ms.count());
}

std::cv_status
exec_wait(const xrt_core::device* device, const std::chrono::milliseconds& timeout_ms)
{
  auto kdev = get_kds_device_or_error(device);
  return kdev->exec_wait(timeout_ms.count());
}

// Start unmanaged command execution.  The command must be explicitly
// tested for completion, either by actively polling command state or
// calling unmanaged wait
void
unmanaged_start(xrt_core::command* cmd)
{
  auto kdev = get_kds_device(cmd);
  kdev->exec_buf(cmd);
}

// Start managed command execution.   The command is monitored
// for completion and notified when completed.  It is undefined
// behavior to call unmanaged_wait for a managed command.  While
// wait will work, the commmand cannot be immediately re-executed
// until it has completed.
void
managed_start(xrt_core::command* cmd)
{
  auto kdev = get_kds_device(cmd);
  kdev->launch(cmd);
}

// Alias for managed_start
void
schedule(xrt_core::command* cmd)
{
  auto kdev = get_kds_device(cmd);
  kdev->launch(cmd);
}

void
start()
{}

void
finish(const xrt_core::device* device)
{
  remove_device(device);
}

void
stop()
{
  XRT_DEBUGF("-> xrt_core::kds::stop()\n");

  // Ensure all threads are joined prior to other cleanup
  // This is used by OpenCL code path before it deletes the
  // global platform and takes care of completing outstanding
  // event synchronization for commands.

  // Wait for all devices to become idle, or force stop them
  // if application relies on static destruction where order is
  // undefined
  wait_while_devices();   // all devices must be done
  stop_monitor_threads(); // stop all idle threads

  XRT_DEBUGF("<- xrt_core::kds::stop()\n");
}

// Create and initialize a kds_device object from a core device.
void
init(xrt_core::device* device)
{
  get_kds_device(device);
}

}} // kds,xrt_core
