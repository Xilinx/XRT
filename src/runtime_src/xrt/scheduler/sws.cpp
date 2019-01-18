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

/**
 * XRT software scheduler (when not using MB embedded scheduler)
 *
 * This is a software model of the firmware for the embedded
 * scheduler.  It is used for DSAs without MB and for emulation.
 */

#include "xrt/config.h"
#include "xrt/util/debug.h"
#include "xrt/util/thread.h"
#include "xrt/util/task.h"
#include "command.h"
#include <limits>
#include <bitset>
#include <vector>
#include <list>
#include <mutex>
#include <condition_variable>

namespace {

////////////////////////////////////////////////////////////////
// Convenience types for readability
////////////////////////////////////////////////////////////////
using size_type = uint32_t;
using addr_type = uint32_t;
using value_type = uint32_t;
using command_type = std::shared_ptr<xrt::command>;

////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////
const size_type max_cus = 128;
using bitmask_type = std::bitset<max_cus>;

// FFA  handling
const size_type CONTROL_AP_START=1;
const size_type CONTROL_AP_DONE=2;
const size_type CONTROL_AP_IDLE=4;

////////////////////////////////////////////////////////////////
// Command opcodes
////////////////////////////////////////////////////////////////
const value_type CMD_START_KERNEL = 0;
const value_type CMD_CONFIGURE = 1;

////////////////////////////////////////////////////////////////
// Configuarable constants
////////////////////////////////////////////////////////////////
// Actual number of cus
static size_type num_cus = 0;

// CU base address
static addr_type cu_base_address = 0x0;

// CU offset (addone is 32k (1<<15), OCL is 4k (1<<12))
static size_type cu_offset = 12;

// Enable features via  configure_mb
static value_type cu_trace_enabled = 0;

// Mapping from cu_idx to its base address
static std::vector<uint32_t> cu_addr_map;

////////////////////////////////////////////////////////////////
// Helper functions for extracting command header information
////////////////////////////////////////////////////////////////
/**
 * Command opcode [27:23]
 */
inline value_type
opcode(value_type header_value)
{
  return (header_value >> 23) & 0x1F;
}

/**
 * Command header [22:12] is payload size
 */
inline size_type
payload_size(value_type header_value)
{
  return (header_value >> 12) & 0x7FF;
}

/**
 * Command header [11:10] is extra cu masks.
 */
inline size_type
cu_masks(value_type header_value)
{
  return 1 + ((header_value >> 10) & 0x3);
}

/**
 * Size of regmap is payload size (n) minus the number of cu_masks
 */
inline size_type
regmap_size(value_type header_value)
{
  return payload_size(header_value) - cu_masks(header_value);
}

/**
 * Convert cu idx into cu address
 */
inline addr_type
cu_idx_to_addr(size_type cu_idx)
{
  return cu_addr_map[cu_idx];
}

struct slot_info
{
  command_type cmd;
  xrt::device* device = nullptr;
  bitmask_type cus;

  // Last command header read from slot in command queue
  // Last 4 bits of header are used for slot status per mb state
  // new     [0x1]: the command is in new state per host
  // queued  [0x2]: the command is queued in MB.
  // running [0x3]: the command is running
  // free    [0x4]: the command slot is free
  value_type header_value = 0;

  slot_info(command_type xcmd)
    : cmd(std::move(xcmd)), device(cmd->get_device()), header_value(cmd->get_header())
  {}

  unsigned int
  get_uid() const
  {
    return cmd->get_uid();
  }

  xrt::command::packet_type&
  get_packet()
  {
    return cmd->get_packet();
  }

  void start(size_type cu)
  {
    // update cus to reflect running cu
    cus.reset();
    cus.set(cu);

    // If cu tracing is enabled then update command packet cumask
    // to reflect running cu prior to invoking the command callback
    if (cu_trace_enabled) {
      auto& packet = get_packet();
      auto cumasks = cu_masks(header_value);
      for (size_type i=0; i<cumasks; ++i) {
        packet[1+i] = ((cus>>(sizeof(value_type)*8*i)).to_ulong()) & 0xFFFFFFFF;
      }
    }

    // Invoke command callback
    cmd->notify(ERT_CMD_STATE_RUNNING);
  }
};

// Command notification is threaded through task queue
// and notifier.  This allows the scheduler to continue
// while host callback can be processed in the background
static xrt::task::queue notify_queue;
static std::thread notifier;
static bool threaded_notification = true;

static std::list<slot_info> command_queue;

// Fixed sized map from cu_idx -> slot_info
static const slot_info* cu_slot_usage[max_cus];

// Bitmask indicating status of CUs. (0) idle, (1) running.
// Only 'num_cus' lower bits are used
static bitmask_type cu_status;

// Track runtime of each cu
static uint64_t cu_total_runtime[max_cus] = {0};
static uint64_t cu_start_time[max_cus] = {0};
static uint64_t cu_stop_time[max_cus] = {0};


/**
 * MB configuration
 */
static void
setup()
{
  command_queue.clear();

  // Initialize cu_slot_usage
  for (size_type i=0; i<num_cus; ++i) {
    cu_slot_usage[i] = nullptr;
    cu_total_runtime[i] = 0;
    cu_start_time[i] = 0;
    cu_stop_time[i] = 0;
  }
}

/**
 * Notify host of command completion
 *
 * Notification is threaded so that the scheduler can proceeed while
 * host side does book keeping
 *
 * To turn off threading, simply call cmd->done() directly in function.
 */
static void
notify_host(slot_info* slot)
{
  // notify host (update host status register)
  XRT_DEBUGF("notify_host(%d)\n",slot->get_uid());

  if (!threaded_notification) {
    slot->cmd->notify(ERT_CMD_STATE_COMPLETED);
    return;
  }

  // It is vital that cmd is kept alive through reference counting
  // by being bound to the notify call back argument. This is
  // because the scheduler itself will remove the command from its
  // queue once it is complete and threading doesn't ensure notify
  // is called first.
  auto notify = [](command_type cmd) {
    cmd->notify(ERT_CMD_STATE_COMPLETED);
  };

  xrt::task::createF(notify_queue,notify,slot->cmd);
}

/**
 * Configure a CU at argument address
 *
 * Write register map to CU control register at address
 *
 * @param cu_addr
 *  Address of CU control register
 * @param regmap_addr
 *  The address of the register map to copy into the CU control
 *  register
 * @param regmap_size
 *  The size of register map in 32 bit words
 */
inline void
configure_cu(slot_info* slot, size_type cu)
{
  auto cu_addr = cu_idx_to_addr(cu);
  auto size = regmap_size(slot->header_value);

  XRT_DEBUGF("configuring cu(%d) at addr(0%x)\n",cu,cu_addr);

  // data past header and cu_masks
  auto regmap = slot->get_packet().data() + 1 + cu_masks(slot->header_value);

  // write register map, starting at base + 0xC
  // 0x4, 0x8 used for interrupt, which is initialized in setu
  slot->device->write_register(cu_addr,regmap,size*4);

  // start cu
  const_cast<uint32_t*>(regmap)[0] = 1;
  slot->device->write_register(cu_addr,regmap,size*4);
}

/**
 * Start a cu for command in slot
 *
 * @param slot_idx
 *  Index of command
 * @return
 *  True of a CU was started, false otherwise
 */
static bool
start_cu(slot_info* slot)
{
  auto cus = slot->cus;

  // Check all CUs against argument cus mask and against cu_status
  for (size_type cu=0; cu<num_cus; ++cu) {
    if (cus.test(cu) && !cu_status.test(cu)) {
      slot->start(cu);           // note that slot is starting on cu
      configure_cu(slot,cu);
      cu_status.flip(cu);        // toggle cu status bit, it is now busy
      cu_slot_usage[cu] = slot;
      return true;
    }
  }
  return false;
}

/**
 * Check CU status
 *
 * CU to check is indicated by bit position in argument cu_mask.
 *
 * If CU is done, then host status register is updated accordingly
 * and internal cu_status register that tracks running CUs is toggled
 * at corresponding position.
 *
 * @param cu_mask
 *   A bitmask with 1 bit set. Position of bit indicates the CU to check
 * @return
 *   True if CU is done, false otherwise
 */
static bool
check_cu(slot_info* slot,bool wait=false)
{
  auto device = slot->device;
  auto& cu_mask = slot->cus;

  // find cu idx in mask
  size_type cu_idx = 0;
  for (; !cu_mask.test(cu_idx); ++cu_idx);
  XRT_ASSERT(cu_idx < num_cus,"bad cu idx");
  XRT_ASSERT(cu_status.test(cu_idx),"cu wasn't started");
  auto cu_addr = cu_idx_to_addr(cu_idx);
  value_type ctrlreg = 0;

  do {
    device->read_register(cu_addr,&ctrlreg,4);
    if (ctrlreg & (CONTROL_AP_IDLE | CONTROL_AP_DONE)) {
      cu_status.flip(cu_idx);
      cu_slot_usage[cu_idx] = nullptr;
      return true;
    }
  } while (wait);

  return false;
}

/**
 * Check if queue is idle except for command in argument slot
 */
static bool
check_idle_prereq(slot_info* slot)
{
  auto end=command_queue.end();
  for (auto itr=command_queue.begin(); itr!=end; ++itr) {
    auto s = &(*itr);
    if (s==slot)
      continue;
    if ((s->header_value & 0xF) != 0x4) {
      XRT_DEBUGF("slot(%d) is busy\n",s->get_uid());
      return false;
    }
  }

  return true;
}


/**
 * Configure MB and peripherals
 *
 * Wait for CONFIGURE_MB in specified slot, then configure as
 * requested.
 *
 * This function is used in two different scenarios:
 *  1. MB reset/startup, in which case the CONFIGURE_MB is guaranteed
 *     to be in a slot at default slot offset (4K), most likely slot 0.
 *  2. During regular scheduler loop, in which case the CONFIGURE_MB
 *     packet is at an arbitrary slot location.   In this scenario, the
 *     function may return (false) without processing the command if
 *     other commands are currently executing; this is to avoid hardware
 *     lockup.
 *
 * @param slot_idx
 *   The slot index with the CONFIGURE_MB command
 * @return
 *   True if CONFIGURE_MB packet was processed, false otherwise
 */
static bool
configure(slot_info* slot)
{
  // Ignore the CONFIGURE packet if any commands are
  // currently being processed.  The main scheduler loop
  // will revisit the CONFIGURE packet again in case 2).
  if (!check_idle_prereq(slot))
    return false;

  XRT_DEBUGF("configure found)\n");
  XRT_DEBUGF("slot(%d) [new->queued]\n",slot->get_uid());
  XRT_DEBUGF("slot(%d) [queued->running]\n",slot->get_uid());

  auto& packet = slot->get_packet();
  num_cus=packet[2];
  cu_offset=packet[3];
  cu_base_address=packet[4];

  // Features
  auto features = packet[5];
  cu_trace_enabled = features & 0x8;

  // (Re)initilize MB
  setup();

  // notify host
  notify_host(slot);

  slot->header_value = (slot->header_value & ~0xF) | 0x4; // free
  XRT_DEBUGF("slot(%d) [running->free]\n",slot->get_uid());

  return true;
}

/**
 * Process special command.
 *
 * Special commands are not performace critical
 *
 * @return true
 *   If command was processed, false otherwise
 */
static bool
process_special_command(slot_info* slot, size_type opcode)
{
  if (opcode==CMD_CONFIGURE)
    return configure(slot);
  return false;
}

static std::mutex s_mutex;
static std::condition_variable s_work;
static bool s_stop=false;
static std::vector<command_type> s_cmds;

/**
 * Main routine executed by embedded scheduler loop
 *
 * For each command slot do
 *  1. If status is free (0x4), then read new command header
 *     Status remains free (0x4), or transitions to new (0x1)
 *  2. If status is new (0x1), then read CUs in command
 *     Status transitions to queued (0x2)
 *  3. If status is queued (0x2), then start command on available CU
 *     Status remains queued if no CUs available, or transitions to running (0x3)
 *  4. If status is running (0x4), then check CU status
 *     Status remains running (0x4) if CU is still running, or
 *     transitions to free if CU is done
 */
static void
scheduler_loop()
{
  // Basic setup will be changed by configure_mb, but is necessary
  // for even configure_mb() to work.
  setup();

  while (1) {

    {
      std::unique_lock<std::mutex> lk(s_mutex);
      while (!s_stop && command_queue.empty() && s_cmds.empty())
        s_work.wait(lk);

      if (s_stop) {
        if (!command_queue.empty() || !s_cmds.empty())
          throw std::runtime_error("software scheduler stopping while there are active commands");
        break;
      }

      // copy new commands to pending list
      std::copy(s_cmds.begin(),s_cmds.end(),std::back_inserter(command_queue));
      s_cmds.clear();
    } // lk scope

    // iterate commands
    auto end = command_queue.end();
    auto nitr = command_queue.begin();
    for (auto itr=nitr; itr!=end; itr=nitr) {
      auto slot = &(*itr);

      if ((slot->header_value & 0xF) == 0x1) { // new
        auto opc = opcode(slot->header_value);
        if (opc!=CMD_START_KERNEL) { // Non performance critical command
          process_special_command(slot,opc);
          continue;
        }

        // Extract and cache cumask from cmd
        size_type cumasks = cu_masks(slot->header_value);
        for (size_type i=0; i<cumasks; ++i) {
          auto& payload = slot->get_packet();
          bitmask_type mask(payload[1+i]);
          slot->cus |= (mask<<sizeof(value_type)*8*i);
        }

        slot->header_value = (slot->header_value & ~0xF) | 0x2; // queued
        XRT_DEBUGF("slot(%d) [new->queued]\n",slot->get_uid());
      }

      if ((slot->header_value & 0xF) == 0x2) { // queued
        // queued command, start if any of cus is ready
        if (start_cu(slot)) { // started
          slot->header_value |= 0x1; // running (0x2->0x3)
          XRT_DEBUGF("slot(%d) [queued->running]\n",slot->get_uid());
        }
      }

      if ((slot->header_value & 0xF) == 0x3) { // running
        // running command, check its cu status
        if (check_cu(slot,false)) {
          notify_host(slot);
          slot->header_value = (slot->header_value & ~0xF) | 0x4; // free
          XRT_DEBUGF("slot(%d) [running->free]\n",slot->get_uid());
        }
      }

      if ((slot->header_value & 0xF) == 0x4) { // free
        nitr = command_queue.erase(itr);
        end = command_queue.end();
        continue;
      }

      nitr = ++itr;

    }
  } // while
}

static bool s_running=false;
static std::thread s_scheduler;


} // namespace

namespace xrt { namespace sws {

void
schedule(const command_type& cmd)
{
  std::lock_guard<std::mutex> lk(s_mutex);
  s_cmds.push_back(cmd);
  s_work.notify_one();
}

void
start()
{
  if (s_running)
    throw std::runtime_error("sws command scheduler is already started");

  std::lock_guard<std::mutex> lk(s_mutex);
  s_scheduler = std::move(xrt::thread(scheduler_loop));
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
    s_stop=true;
  }

  s_work.notify_one();
  s_scheduler.join();

  if (threaded_notification) {
    // wait for notifier to drain
    while (notify_queue.size()) {
      XRT_DEBUG(std::cout,"waiting for notifier to drain\n");
    }

    notify_queue.stop();
    notifier.join();
  }

  s_running = false;
}

void
init(xrt::device*, size_t, size_t cus, size_t cuoffset, size_t cubase, const std::vector<uint32_t>& cu_amap)
{
  num_cus = cus;
  cu_base_address = cubase;
  cu_offset = cuoffset;
  cu_trace_enabled = xrt::config::get_profile();
  cu_addr_map = cu_amap;
}

}} // sws,xrt
