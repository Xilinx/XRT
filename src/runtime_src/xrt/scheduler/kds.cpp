/**
 * Copyright (C) 2018 Xilinx, Inc
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
//#define KDS_VERBOSE
#if defined(KDS_VERBOSE) && !defined(XRT_VERBOSE)
# define XRT_VERBOSE
#endif

/**
 * XRT Kernel Driver command scheduler (when using kernel driver scheduling)
 */
#include "scheduler.h"
#include "xrt/util/error.h"
#include "xrt/util/debug.h"
#include "xrt/util/time.h"
#include "xrt/util/task.h"
#include "xrt/device/device.h"
#include "ert.h"
#include "command.h"
#include "core/common/thread.h"

#include <memory>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <thread>
#include <list>
#include <map>

namespace {

using command_type = std::shared_ptr<xrt::command>;
using command_queue_type = std::list<command_type>;

////////////////////////////////////////////////////////////////
// Command notification is threaded through task queue
// and notifier.  This allows the scheduler to continue
// while host callback can be processed in the background
////////////////////////////////////////////////////////////////
static xrt::task::queue notify_queue;
static std::thread notifier;
static bool threaded_notification = true;

////////////////////////////////////////////////////////////////
// Main command monitor interfacing to embedded MB scheduler
////////////////////////////////////////////////////////////////
static std::mutex s_mutex;
static std::condition_variable s_work;
static bool s_running = false;
static bool s_stop = false;
static std::exception_ptr s_exception;
static std::map<const xrt::device*, command_queue_type> s_device_cmds;
static std::map<const xrt::device*, std::thread> s_device_monitor_threads;

inline bool
is_51_dsa(const xrt::device* device)
{
  auto nm = device->getName();
  return (nm.find("_5_1")!=std::string::npos || nm.find("u200_xdma_201820_1")!=std::string::npos);
}

inline ert_cmd_state
get_command_state(const command_type& cmd)
{
  ert_packet* epacket = xrt::command_cast<ert_packet*>(cmd.get());
  return static_cast<ert_cmd_state>(epacket->state);
}

inline bool
is_command_done(const command_type& cmd)
{
  return get_command_state(cmd) >= ERT_CMD_STATE_COMPLETED;
}

static bool
check(const command_type& cmd)
{
  if (!is_command_done(cmd))
    return false;

  XRT_DEBUG(std::cout,"xrt::kds::command(",cmd->get_uid(),") [running->done]\n");
  if (!threaded_notification) {
    cmd->notify(ERT_CMD_STATE_COMPLETED);
    return true;
  }

  auto notify = [](command_type c) {
    c->notify(ERT_CMD_STATE_COMPLETED);
  };

  xrt::task::createF(notify_queue,notify,cmd);
  return true;
}

static void
launch(command_type cmd)
{
  XRT_DEBUG(std::cout,"xrt::kds::command(",cmd->get_uid(),") [new->submitted->running]\n");

  auto device = cmd->get_device();
  auto& submitted_cmds = s_device_cmds[device]; // safe since inserted in init

  command_queue_type::const_iterator pos;

  // Store command so completion can be tracked.  Make sure this is
  // done prior to exec_buf as exec_wait can otherwise be missed.
  {
    std::lock_guard<std::mutex> lk(s_mutex);
    pos = submitted_cmds.insert(submitted_cmds.end(),cmd);
    s_work.notify_all();
  }

  // Submit the command
  auto exec_bo = cmd->get_exec_bo();
  try {
    device->exec_buf(exec_bo);
  }
  catch (...) {
    // Remove the pending command
    std::lock_guard<std::mutex> lk(s_mutex);
    assert(get_command_state(cmd)==ERT_CMD_STATE_NEW);
    submitted_cmds.erase(pos);
    throw;
  }
}

static void
monitor_loop(const xrt::device* device)
{
  unsigned long loops = 0;           // number of outer loops
  unsigned long sleeps = 0;          // number of sleeps

  // thread safe access, since guaranteed to be inserted in init
  auto& submitted_cmds = s_device_cmds[device];

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
      while (device->exec_wait(1000)==0) ;

      std::lock_guard<std::mutex> lk(s_mutex);
      auto end = submitted_cmds.end();
      for (auto itr=submitted_cmds.begin(); itr!=end; ) {
        auto& cmd = (*itr);
        if (check(cmd)) {
          itr = submitted_cmds.erase(itr);
          end = submitted_cmds.end();
        }
        else {
          ++itr;
        }
      }
    }
  }
}


static void
monitor(const xrt::device* device)
{
  try {
    monitor_loop(device);
  }
  catch (const std::exception& ex) {
    std::string msg = std::string("kds command monitor died unexpectedly: ") + ex.what();
    xrt::send_exception_message(msg.c_str());
    s_exception = std::current_exception();
  }
  catch (...) {
    xrt::send_exception_message("kds command monitor died unexpectedly");
    s_exception = std::current_exception();
  }
}

} // namespace


namespace xrt { namespace kds {

void
schedule(const command_type& cmd)
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
    notifier = std::move(xrt_core::thread(xrt::task::worker,std::ref(notify_queue)));
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
init(xrt::device* device)
{
  // create a submitted command queue for this device if necessary,
  // create a command monitor thread for this device if necessary
  std::lock_guard<std::mutex> lk(s_mutex);
  auto itr = s_device_monitor_threads.find(device);
  if (itr==s_device_monitor_threads.end()) {
    XRT_DEBUG(std::cout,"creating monitor thread and queue for device '",device->getName(),"'\n");
    s_device_cmds.emplace(device,command_queue_type());
    s_device_monitor_threads.emplace(device,xrt_core::thread(::monitor,device));
  }
}

}} // kds,xrt
