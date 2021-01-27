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

namespace {

using command_queue_type = std::vector<xrt_core::command*>;

////////////////////////////////////////////////////////////////
// Main command monitor interfacing to embedded MB scheduler
////////////////////////////////////////////////////////////////
static std::mutex s_mutex;
static std::condition_variable s_work;
static bool s_running = false;
static bool s_stop = false;
static std::exception_ptr s_exception;
static std::map<const xrt_core::device*, command_queue_type> s_device_cmds;
static std::map<const xrt_core::device*, std::thread> s_device_monitor_threads;

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

static void
notify_host(xrt_core::command* cmd)
{
  auto state = get_command_state(cmd);

  XRT_DEBUGF("xrt_core::kds::command(%d), [running->done]\n", cmd->get_uid());
  auto retain = cmd->shared_from_this();
  cmd->notify(state);
}

static void
launch(xrt_core::command* cmd)
{
  XRT_DEBUGF("xrt_core::kds::command(%d) [new->submitted->running]\n", cmd->get_uid());

  auto device = cmd->get_device();
  auto& submitted_cmds = s_device_cmds[device]; // safe since inserted in init

  // Store command so completion can be tracked.  Make sure this is
  // done prior to exec_buf as exec_wait can otherwise be missed.
  // See detailed explanation in monitor loop.
  {
    std::lock_guard<std::mutex> lk(s_mutex);
    submitted_cmds.push_back(cmd);
  }

  // Submit the command
  try {
    device->exec_buf(cmd->get_exec_bo());
  }
  catch (...) {
    // Remove the pending command
    std::lock_guard<std::mutex> lk(s_mutex);
    assert(get_command_state(cmd)==ERT_CMD_STATE_NEW);
    if (!submitted_cmds.empty())
      submitted_cmds.pop_back();
    throw;
  }

  // This is somewhat expensive, it is better to have this after the
  // exec_buf call so that actual execution doesn't have to wait.
  s_work.notify_one();
}

static void
monitor_loop(const xrt_core::device* device)
{
  // thread safe access, since guaranteed to be inserted in init
  auto& submitted_cmds = s_device_cmds[device];
  std::vector<xrt_core::command*> busy_cmds;
  std::vector<xrt_core::command*> running_cmds;

  while (1) {
    // Larger wait synchronized with launch()
    {
      std::unique_lock<std::mutex> lk(s_mutex);
      while (!s_stop && running_cmds.empty() && submitted_cmds.empty())
        s_work.wait(lk);
    }

    if (s_stop)
      return;

    // Finer wait
    while (device->exec_wait(1000)==0) {}

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
      std::lock_guard<std::mutex> lk(s_mutex);
      std::copy(submitted_cmds.begin(), submitted_cmds.end(), std::back_inserter(running_cmds));
      submitted_cmds.clear();
    }
    // At this point running_cmds is guaranteed to contain the
    // command(s) for which exec_wait returned.

#if 0
    // Out of order processing
    auto size = running_cmds.size();
    for (size_t idx=0; idx<size; ++idx) {
      auto cmd = running_cmds[idx];
      if (!completed(cmd))
        continue;

      notify_host(cmd);
      auto last = running_cmds.back();
      if (last != cmd)
        running_cmds[idx--] = last;
      running_cmds.pop_back();
      --size;
    }
#endif

#if 1
    // Preserve order of processing
    for (auto cmd : running_cmds) {
      if (completed(cmd))
        notify_host(cmd);
      else
        busy_cmds.push_back(cmd);
    }

    running_cmds.swap(busy_cmds);
    busy_cmds.clear();
#endif
  } // while (1)
}

static void
monitor(const xrt_core::device* device)
{
  try {
    monitor_loop(device);
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

} // namespace


namespace xrt_core { namespace kds {

void
schedule(xrt_core::command* cmd)
{
  return launch(cmd);
}

void
start()
{
  if (s_running)
    throw std::runtime_error("kds command monitor is already started");

  std::lock_guard<std::mutex> lk(s_mutex);
  s_running = true;
}

void
stop()
{
  if (!s_running)
    return;

  {
    std::lock_guard<std::mutex> lk(s_mutex);
    s_stop = true;
  }

  s_work.notify_all();
  for (auto& e : s_device_monitor_threads)
    e.second.join();

  s_running = false;
}

void
init(xrt_core::device* device)
{
  // create a submitted command queue for this device if necessary,
  // create a command monitor thread for this device if necessary
  std::lock_guard<std::mutex> lk(s_mutex);
  auto itr = s_device_monitor_threads.find(device);
  if (itr==s_device_monitor_threads.end()) {
    XRT_DEBUGF("creating monitor thread and queue for device\n");
    s_device_cmds.emplace(device,command_queue_type());
    s_device_monitor_threads.emplace(device,xrt_core::thread(::monitor,device));
  }
}

}} // kds,xrt_core
