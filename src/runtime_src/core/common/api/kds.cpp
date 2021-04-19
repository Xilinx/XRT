/**
 * Copyright (C) 2020 Xilinx, Inc
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
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common

#include "exec.h"
#include "ert.h"
#include "command.h"
#include "core/common/device.h"
#include "core/common/thread.h"
#include "core/common/debug.h"

#include <memory>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <list>
#include <map>

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
  void
  exec_wait()
  {
    static thread_local uint64_t thread_exec_wait_call_count = 0;
    std::lock_guard<std::mutex> lk(exec_wait_mutex);

    if (thread_exec_wait_call_count != exec_wait_call_count) {
      // some other thread has called exec_wait and may have
      // covered this thread's commands, synchronize thread
      // local call count and return to caller.
      thread_exec_wait_call_count = exec_wait_call_count;
      return;
    }

    while (device->exec_wait(1000)==0) {}

    // synchronize this thread with total call count
    thread_exec_wait_call_count = ++exec_wait_call_count;
  }

  // exec_wait() - Wait for specific command completion
  //
  // This function is safe to call for managed and unmanaged commands.
  void
  exec_wait(const xrt_core::command* cmd)
  {
    auto pkt = cmd->get_ert_packet();
    while (pkt->state < ERT_CMD_STATE_COMPLETED)
      exec_wait();

    // notify_host is not strictly necessary for unmanaged
    // command execution but provides a central place to update
    // and mark commands as done so they can be re-executed.
    notify_host(const_cast<xrt_core::command*>(cmd), static_cast<ert_cmd_state>(pkt->state));
  }

  // exec_buf() - Submit a command for execution
  //
  // This function is used to schedule unmanaged commands for
  // execution. The execution monitor is by-passed and will be unaware
  // of argument command having been scheduled for execution.
  void
  exec_buf(xrt_core::command* cmd)
  {
    device->exec_buf(cmd->get_exec_bo());
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
      exec_buf(cmd);
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

// Statically allocated kds_device object for each core deviced
static std::map<const xrt_core::device*, std::unique_ptr<kds_device>> kds_devices;

// Get or create kds_device object from core device
static kds_device*
get_kds_device(xrt_core::device* device)
{
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

} // namespace


namespace xrt_core { namespace kds {

// Wait for command completion for unmanaged command execution
void
unmanaged_wait(const xrt_core::command* cmd)
{
  auto kdev = get_kds_device(cmd);
  kdev->exec_wait(cmd);
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
stop()
{
  kds_devices.clear();
}


// Create and initialize a kds_device object from a core device.
void
init(xrt_core::device* device)
{
  get_kds_device(device);
}

}} // kds,xrt_core
