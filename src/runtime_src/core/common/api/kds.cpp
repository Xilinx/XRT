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
#include "core/common/task.h"
#include "core/common/thread.h"

#include <memory>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <thread>
#include <list>
#include <map>

namespace {

using command_queue_type = std::vector<xrt_core::command*>;

////////////////////////////////////////////////////////////////
// Command notification is threaded through task queue
// and notifier.  This allows the scheduler to continue
// while host callback can be processed in the background
////////////////////////////////////////////////////////////////
static xrt_core::task::queue notify_queue;
static std::thread notifier;

// Turn off threaded notification because of overhead
// Not good for simple notification for XRT native kernel APIs
static bool threaded_notification = false;

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

  XRT_DEBUG(std::cout,"xrt_core::kds::command(",cmd->get_uid(),") [running->done]\n");
  if (!threaded_notification) {
    cmd->notify(state);
    return;
  }

  auto notify = [state](xrt_core::command* c) {
    c->notify(state);
  };

  xrt_core::task::createF(notify_queue,notify,cmd);
}

static void
launch(xrt_core::command* cmd)
{
  XRT_DEBUG(std::cout,"xrt_core::kds::command(",cmd->get_uid(),") [new->submitted->running]\n");

  auto device = cmd->get_device();
  auto& submitted_cmds = s_device_cmds[device]; // safe since inserted in init

  command_queue_type::const_iterator pos;

  // Store command so completion can be tracked.  Make sure this is
  // done prior to exec_buf as exec_wait can otherwise be missed.
  {
    std::lock_guard<std::mutex> lk(s_mutex);
    submitted_cmds.push_back(cmd);
    s_work.notify_all();
  }

  // Submit the command
  try {
    device->exec_buf(cmd->get_exec_bo());
  }
  catch (...) {
    // Remove the pending command
    std::lock_guard<std::mutex> lk(s_mutex);
    assert(get_command_state(cmd)==ERT_CMD_STATE_NEW);
    submitted_cmds.pop_back();
    throw;
  }
}

static void
monitor_loop(const xrt_core::device* device)
{
  unsigned long loops = 0;           // number of outer loops
  unsigned long sleeps = 0;          // number of sleeps

  // thread safe access, since guaranteed to be inserted in init
  auto& submitted_cmds = s_device_cmds[device];
  std::vector<xrt_core::command*> completed_cmds;

  while (1) {
    ++loops;

    {
      {
        std::unique_lock<std::mutex> lk(s_mutex);

        // Larger wait
        while (!s_stop && submitted_cmds.empty()) {
          ++sleeps;
          s_work.wait(lk);
        }
      }

      if (s_stop)
        return;

      // Finer wait
      while (device->exec_wait(1000)==0) {}

      {
        std::lock_guard<std::mutex> lk(s_mutex);
        auto size = submitted_cmds.size();
        for (size_t idx=0; idx<size; ++idx) {
          auto cmd = submitted_cmds[idx];
          if (!completed(cmd))
            continue;

          completed_cmds.push_back(cmd);
          auto last = submitted_cmds.back();
          if (last != cmd)
            submitted_cmds[idx--] = last;
          submitted_cmds.pop_back();
          --size;
        }
      }

      // Notify host outside lock
      for (auto cmd : completed_cmds)
        notify_host(cmd);
      completed_cmds.clear();
    }
  }
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
  if (threaded_notification)
    notifier = std::move(xrt_core::thread(xrt_core::task::worker,std::ref(notify_queue)));
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

  notify_queue.stop();
  if (threaded_notification)
    notifier.join();

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
    XRT_DEBUG(std::cout,"creating monitor thread and queue for device\n");
    s_device_cmds.emplace(device,command_queue_type());
    s_device_monitor_threads.emplace(device,xrt_core::thread(::monitor,device));
  }
}

}} // kds,xrt_core
