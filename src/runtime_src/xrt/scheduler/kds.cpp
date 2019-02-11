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
#include "xrt/config.h"
#include "xrt/util/error.h"
#include "xrt/util/thread.h"
#include "xrt/util/debug.h"
#include "xrt/util/time.h"
#include "xrt/util/task.h"
#include "xrt/device/device.h"
#include "driver/include/ert.h"
#include "command.h"

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

inline bool
is_command_done(const command_type& cmd)
{
  ert_packet* epacket = xrt::command_cast<ert_packet*>(cmd.get());
  return epacket->state >= ERT_CMD_STATE_COMPLETED;
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

  // Store command so completion can be tracked.  Make sure this is
  // done prior to exec_buf as exec_wait can otherwise be missed.
  {
    std::lock_guard<std::mutex> lk(s_mutex);
    submitted_cmds.push_back(cmd);
    s_work.notify_all();
  }

  // Submit the command
  auto exec_bo = cmd->get_exec_bo();
  device->exec_buf(exec_bo);
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

static std::thread s_monitor;

} // namespace


namespace xrt { namespace kds {

void
schedule(const command_type& cmd)
{
  launch(cmd);
}

void
start()
{
  if (s_running)
    throw std::runtime_error("kds command monitor is already started");

  std::lock_guard<std::mutex> lk(s_mutex);
  if (threaded_notification)
    notifier = std::move(xrt::thread(xrt::task::worker,std::ref(notify_queue)));
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
init(xrt::device* device, size_t regmap_size, bool cu_isr, size_t num_cus, size_t cu_offset, size_t cu_base_addr, const std::vector<uint32_t>& cu_addr_map)
{
  auto cudma = xrt::config::get_ert_cudma();
  if (cudma && regmap_size>=0x210 && is_51_dsa(device)) {
    // bug in cudma.c HW
    xrt::message::send(xrt::message::severity_level::WARNING,
                       "Disabling CUDMA. Kernel register map size '"
                       + std::to_string(regmap_size)
                       + " bytes' exceeds CUDMA limit '"
                       + std::to_string(0x210)
                       + " bytes'.");
    cudma = false;
  }

  auto configure = std::make_shared<command>(device,ERT_CONFIGURE);
  auto epacket = xrt::command_cast<ert_configure_cmd*>(configure.get());

  // variables (one word each)
  epacket->slot_size = xrt::config::get_ert_slotsize();
  epacket->num_cus = num_cus;
  epacket->cu_shift = cu_offset;
  epacket->cu_base_addr = cu_base_addr;

  // features (one word) per sdaccel.ini
  epacket->ert     = xrt::config::get_ert();
  epacket->polling = xrt::config::get_ert_polling();
  epacket->cu_dma  = cudma;
  epacket->cu_isr  = cu_isr && xrt::config::get_ert_cuisr();
  epacket->cq_int  = xrt::config::get_ert_cqint();

  // cu addr map
  std::copy(cu_addr_map.begin(), cu_addr_map.end(), epacket->data);

  // payload size
  epacket->count = 5 + cu_addr_map.size();

  XRT_DEBUG(std::cout,"configure scheduler(",getpid(),")\n");
  auto exec_bo = configure->get_exec_bo();
  device->exec_buf(exec_bo);

  // wait for command to complete
  while (!is_command_done(configure))
    while (device->exec_wait(1000)==0) ;

  // create a submitted command queue for this device if necessary,
  // create a command monitor thread for this device if necessary
  std::lock_guard<std::mutex> lk(s_mutex);
  auto itr = s_device_monitor_threads.find(device);
  if (itr==s_device_monitor_threads.end()) {
    XRT_DEBUG(std::cout,"creating monitor thread and queue for device '",device->getName(),"'\n");
    s_device_cmds.emplace(device,command_queue_type());
    s_device_monitor_threads.emplace(device,xrt::thread(::monitor,device));
  }

  XRT_DEBUG(std::cout,"configure complete(",getpid(),")\n");
}

}} // kds,xrt
