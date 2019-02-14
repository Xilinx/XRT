/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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
 * XRT software scheduler in user space.
 *
 * This is a software model of the kds scheduler.  Primarily
 * for debug and bring up.
 */

#include "xrt/config.h"
#include "xrt/util/debug.h"
#include "xrt/util/thread.h"
#include "xrt/util/task.h"
#include "driver/include/ert.h"
#include "command.h"
#include <limits>
#include <bitset>
#include <vector>
#include <list>
#include <map>
#include <mutex>
#include <condition_variable>

namespace {

static bool
emulation_mode()
{
  static bool val = (std::getenv("XCL_EMULATION_MODE") != nullptr);
  return val;
}

////////////////////////////////////////////////////////////////
// Convenience types for readability
////////////////////////////////////////////////////////////////
using idx_type    = uint32_t;
using size_type   = uint32_t;
using addr_type   = uint32_t;
using value_type  = uint32_t;
using cmd_ptr     = std::shared_ptr<xrt::command>;

////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////
const size_type MAX_CUS = 128;
using cu_bitset_type = std::bitset<MAX_CUS>;

const size_type no_index = std::numeric_limits<size_type>::max();

const size_type MAX_SLOTS = 128;

// FFA  handling
const value_type AP_START    = 0x1;
const value_type AP_DONE     = 0x2;
const value_type AP_IDLE     = 0x4;
const value_type AP_READY    = 0x8;
const value_type AP_CONTINUE = 0x10;

////////////////////////////////////////////////////////////////
// Command notification is threaded through task queue
// and notifier.  This allows the scheduler to continue
// while host callback can be processed in the background
////////////////////////////////////////////////////////////////
static xrt::task::queue notify_queue;
static std::thread notifier;
static bool threaded_notification = true;

////////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////////
class xocl_scheduler;
class xocl_cmd;
class exec_core;


////////////////////////////////////////////////////////////////
// exec_core, represents a device execution core
////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
// class xocl_cmd wraps an xrt command object with additional
// bookeeping data.
//
// @m_cmd: xrt command object
// @m_ecmd: xrt command packet data
// @m_kcmd: xrt command packet data cast to start kernel cmd
// @m_exec: execution core on which this command executes
// @m_cus: bitset representing the CUs ths cmd can execute on
// @m_state: current state of this command
// @slotidx: command queue slot when command is submitted
// @cuidx: index of CU executing this command
////////////////////////////////////////////////////////////////
class xocl_cmd
{
private:
  cmd_ptr m_cmd;
  union {
    ert_packet* m_ecmd;
    //ert_df_kernel_cmd* m_kcmd;
    ert_start_kernel_cmd* m_kcmd;
  };
  exec_core* m_exec;
  cu_bitset_type m_cus;
  ert_cmd_state m_state;

  size_type m_uid;

public:
  size_type slotidx = no_index;
  size_type cuidx = no_index;

  xocl_cmd(exec_core* ec, cmd_ptr cmd)
    : m_cmd(cmd), m_ecmd(m_cmd->get_ert_cmd<ert_packet*>()), m_exec(ec), m_state(ERT_CMD_STATE_NEW)
  {
    static size_type count = 0;
    m_uid = count++;
    if (m_ecmd->opcode==ERT_START_KERNEL) {
      m_cus |= m_kcmd->cu_mask;
      for (size_type i=0; i<m_kcmd->extra_cu_masks; ++i) {
        cu_bitset_type mask(m_kcmd->data[i]);
        m_cus |= (mask<<sizeof(value_type)*8*i);
      }
    }
  }

  size_type
  get_uid() const
  {
    return m_uid;
  }

  // Notify host of command completion
  void
  notify_host() const
  {
    if (!threaded_notification) {
      m_cmd->notify(ERT_CMD_STATE_COMPLETED);
      return;
    }

    // It is vital that cmd is kept alive through reference counting
    // by being bound to the notify call back argument. This is
    // because the scheduler itself will remove the command from its
    // queue once it is complete and threading doesn't ensure notify
    // is called first.
    auto notify = [](cmd_ptr cmd) {
      cmd->notify(ERT_CMD_STATE_COMPLETED);
    };

    xrt::task::createF(notify_queue,notify,m_cmd);
  }

  // @return current state of the command object
  ert_cmd_state
  get_state() const
  {
    return m_state;
  }

  // Set the state of the command object
  //
  // The function sets the state of the both the xrt command object
  // and the internal local command state, where latter is used by
  // scheduler internally.
  void
  set_state(ert_cmd_state state)
  {
    m_state = state;
    m_kcmd->state = state;
  }

  // Set only the internal state of the command object.
  //
  // The internal state is used by the scheduler but irrelevant to
  // the xrt command object itself.
  void
  set_int_state(ert_cmd_state state)
  {
    m_state = state;
  }

  // Number of cu masks in this command object.
  //
  // No checking to verify that the command object is a
  // start kernel command.
  //
  // @return
  //  Number of cu masks in this command object
  size_type
  cumasks() const
  {
    return 1 + m_kcmd->extra_cu_masks;
  }


  // Payload size of this command object
  //
  // No checking to verify that the command object is a
  // start kernel command.
  //
  // @return
  //  Size in number of words in command payload
  size_type
  payload_size() const
  {
    return m_kcmd->count;
  }

  // Register map size of this command object
  //
  // No checking to verify that the command object is a
  // start kernel command.
  //
  // @return
  //  Size (number of words) of register map
  size_type
  regmap_size() const
  {
    return payload_size() - cumasks();
  }

  // Register map data
  //
  // No checking to verify that the command object is a
  // start kernel command.
  //
  // @return
  //  Pointer to first word in command register map
  value_type*
  regmap_data() const
  {
    return (m_kcmd->data + m_kcmd->extra_cu_masks);
  }

  // Check if this command can execute on specified CUa
  bool
  has_cu(size_type cu_idx) const
  {
    return m_cus.test(cu_idx);
  }

  // Get the execution core for this command object
  exec_core*
  get_exec() const
  {
    return m_exec;
  }

  // Create a command object
  static std::shared_ptr<xocl_cmd>
  create(exec_core* ec, cmd_ptr cmd)
  {
    return std::make_shared<xocl_cmd>(ec,cmd);
  }
};

using xcmd_ptr = std::shared_ptr<xocl_cmd>;

////////////////////////////////////////////////////////////////
// List of new pending xocl_cmd objects
//
// @s_pending_cmds: populated from user space with new commands for buffer objects
// @s_num_pending: number of pending commands
//
// Scheduler copies pending commands to its private queue when necessary
////////////////////////////////////////////////////////////////
static std::vector<xcmd_ptr> s_pending_cmds;
static std::mutex s_pending_mutex;
static std::atomic<unsigned int> s_num_pending {0};

////////////////////////////////////////////////////////////////
// class xocl_cu represents a compute unit on a device
//
// @running_queue: a fifo representing commands running on this CU
// @xdev: the xrt device with this CU
// @idx: index of this CU
// @addr: base address of this CU
// @ctrlreg: state of the CU (value of AXI-lite control register)
// @done_counter: number of command that have completed (<=running_queue.size())
//
// The CU supports HLS data flow model where running_queue represents
// all the commands that have been started on this CU. The CU is polled
// for AXI-lite status change and when AP_DONE is asserted the done counter
// is incremented to reflect the number of commands in the fifo that have
// completed execution.
//
// New commands can be pushed to the running_queue when the CU has
// asserted AP_READY (=> AP_START is low)
////////////////////////////////////////////////////////////////
class xocl_cu
{
private:
  std::queue<xocl_cmd*> running_queue;
  xrt::device* xdev = nullptr;
  size_type idx = 0;
  value_type addr = 0;

  mutable value_type ctrlreg = 0;
  mutable size_type done_counter = 0;

  void
  poll() const
  {
    XRT_ASSERT(running_queue.size(),"cu wasn't started");
    ctrlreg = 0;

    xdev->read_register(addr,&ctrlreg,4);
    XRT_DEBUGF("sws cu(%d) poll(0x%x)\n",idx,ctrlreg);
    if (ctrlreg & (AP_DONE | AP_IDLE))  { // AP_IDLE check in sw emulation
      ++done_counter;
      XRT_ASSERT(done_counter <= running_queue.size(),"too many dones");
      // acknowledge done
      value_type cont = AP_CONTINUE;
      xdev->write_register(addr,&cont,4);
    }
  }

public:
  xocl_cu(xrt::device* dev, size_type index, value_type baseaddr)
    : xdev(dev), idx(index), addr(baseaddr)
  {}

  // Check if CU is ready to start another command
  //
  // The CU is ready when AP_START is low
  //
  // @return
  //  True if ready, false otherwise
  bool
  ready() const
  {
    if (ctrlreg & AP_START) {
      XRT_DEBUGF("sws ready() is polling cu(%d)\n",idx);
      poll();
    }

    return !(ctrlreg & AP_START);
  }

  // Get the first completed command from the running queue
  //
  // @return
  //   The first command that has completed or nullptr if none
  xocl_cmd*
  get_done() const
  {
    if (!done_counter) {
      XRT_DEBUGF("sws get_done() is polling cu(%d)\n",idx);
      poll();
    }

    return done_counter
      ? running_queue.front()
      : nullptr;
  }

  // Pop the first completed command off of the running queue
  void
  pop_done()
  {
    if (!done_counter)
      return;

    running_queue.pop();
    --done_counter;
  }

  // Start the CU with a new command.
  //
  // The command is pushed onto the running queue
  void
  start(xocl_cmd* xcmd)
  {
    XRT_ASSERT(!(ctrlreg & AP_START),"cu not ready");
    XRT_DEBUGF("configuring cu(%d) at addr(0x%x)\n",idx,addr);

    // data past header and cu_masks
    auto size = xcmd->regmap_size();
    auto regmap = xcmd->regmap_data();

    // write register map, starting at base + 0xC
    // 0x4, 0x8 used for interrupt, which is initialized in setu
    xdev->write_register(addr,regmap,size*4);

    // start cu
    ctrlreg |= 0x1;
    const_cast<uint32_t*>(regmap)[0] = 1;
    if (emulation_mode())
      xdev->write_register(addr,regmap,size*4);
    else
      xdev->write_register(addr,regmap,4);

    running_queue.push(xcmd);
#if 0
    if (running_queue.size()-done_counter > 1)
      XRT_PRINTF("running_queue size %d\n",running_queue.size());
#endif
  }
};


////////////////////////////////////////////////////////////////
// class exec_core: core data struct for command execution on a device
//
// @xdev: the xrt device on which to execute
// @scheduler: scheduler that manages this execution core
// @submit_queue: queue holding command that have been submitted by scheduler
// @slot_status: bitset representing free/busy slots in submit_queue
// @cu_usage: list of CUs managed by this execution core (device)
// @num_slots: number of slots in submit queue
// @num_cus: number of CUs on device
//
// The submit queue reflects the hardware command queue such that
// number of slots is limitted.  Once submit queue is full, the
// scheduler backs off submitting commands to this execution core. The
// size limitation is not a requirement for the software scheduler,
// but makes the behavior closer to actual HW scheduler and shouldn't
// affect performance.
//
// Once a command is started on a CU it is removed from the submit
// queue.  The command is annotated with the CU on which is has been
// started, so scheduler will revisit the command and check for its
// completion.
////////////////////////////////////////////////////////////////
class exec_core
{
  // device
  xrt::device* m_xdev = nullptr;

  // scheduler for this device
  xocl_scheduler* m_scheduler = nullptr;

  // Commands submitted to this device, the queue is slot based
  // and a slot becomes free when its command is started on a CU
  xocl_cmd* submit_queue[MAX_SLOTS] = {nullptr}; // reflects ERT CQ # slots
  std::bitset<MAX_SLOTS> slot_status;

  // Compute units on this device
  std::vector<std::unique_ptr<xocl_cu>> cu_usage;

  size_type num_slots = 0;
  size_type num_cus = 0;

public:
  exec_core(xrt::device* xdev, xocl_scheduler* xs, size_t slots, const std::vector<uint32_t>& cu_amap)
    : m_xdev(xdev), m_scheduler(xs), num_slots(slots), num_cus(cu_amap.size())
  {
    cu_usage.reserve(cu_amap.size());
    for (size_type idx=0; idx<cu_amap.size(); ++idx)
      cu_usage.push_back(std::make_unique<xocl_cu>(xdev,idx,cu_amap[idx]));
  }

  // Scheduler mananging this execution core
  xocl_scheduler*
  get_scheduler() const
  {
    return m_scheduler;
  }

  // Get a free slot index into submit queue
  //
  // @return
  //  First free idx, no no_index if none available
  size_type
  acquire_slot_idx()
  {
    // ffz
    for (size_type idx=0; idx<num_slots; ++idx) {
      if (!slot_status.test(idx)) {
        slot_status.set(idx);
        return idx;
      }
    }
    return no_index;
  }

  // Release a slot index
  void
  release_slot_idx(size_type slot_idx)
  {
    assert(slot_status.test(slot_idx));
    slot_status.reset(slot_idx);
  }

  // Submit a command to this exec core
  //
  // Submit fails if there is no room in submit queue
  //
  // @return
  //   True if submitted successfully, false otherwise
  bool
  submit(xocl_cmd* xcmd)
  {
    if (slot_status.all())
      return false;

    auto slot_idx = acquire_slot_idx();
    if (slot_idx==no_index)
      return false;

    xcmd->slotidx = slot_idx;
    submit_queue[slot_idx]=xcmd;

    return true;
  }

  // Start a command on first available ready CU
  //
  // @return
  //  True if started successfully, false otherwise
  bool
  penguin_start(xocl_cmd* xcmd)
  {
    // Find a ready CU
    for (size_type cuidx=0; cuidx<num_cus; ++cuidx) {
      auto& cu = cu_usage[cuidx];
      if (xcmd->has_cu(cuidx) && cu->ready()) {
        xcmd->cuidx = cuidx;
        cu->start(xcmd);
        return true;
      }
    }
    return false;
  }

  // Start a command on first available ready CU
  //
  // @return
  //  True if started successfully, false otherwise
  bool
  start(xocl_cmd* xcmd)
  {
    if (penguin_start(xcmd)) {
      submit_queue[xcmd->slotidx]=nullptr;
      release_slot_idx(xcmd->slotidx);
      return true;
    }
    return false;
  }

  // Check if a command has completed execution
  //
  // It is precond that command has been started, so the CU that executes
  // the command is indicated by the cuidx in the command.  Simply check
  // if the first completed command in the CU is the argument command
  // and if so pop it off the CU.
  //
  // @return
  //   True if completed, false otherwise
  bool
  penguin_query(xocl_cmd* xcmd)
  {
    // no point to query a command
    auto& cu = cu_usage[xcmd->cuidx];
    if (cu->get_done()==xcmd) {
      cu->pop_done();
      return true;
    }
    return false;
  }

  // Check if a command has completed execution
  bool
  query(xocl_cmd* xcmd)
  {
    return penguin_query(xcmd);
  }
};

////////////////////////////////////////////////////////////////
// class xocl_scheduler: The scheduler data structure
//
// @m_command_queue: all the commands managed by scheduler
//
// The scheduler babysits all commands launched by user. It
// transitions the commands from state to state until the command
// completes.
//
// The scheduler runs on its own thread and manages command execution
// on execution cores.  An execution core is 1-1 with a scheduler, but
// a scheduler can manage any number of cores.  Because the scheduler
// is the only client of an exec_core, and exec_core is the only
// client of xocl_cu, no locking is necessary is any of the data
// structures.  Exception is the pending command list which is copied
// to the scheduler command queue, the pending list is populated by
// user thread, and harvested by scheduler thread.
////////////////////////////////////////////////////////////////
class xocl_scheduler
{
  std::mutex                 m_mutex;
  std::condition_variable    m_work;

  bool                       m_stop = false;
  std::list<xcmd_ptr>        m_command_queue;

  // Copy pending commands into command queue.
  void
  queue_cmds()
  {
    std::lock_guard<std::mutex> lk(s_pending_mutex);
    for (auto itr=s_pending_cmds.begin(); itr!=s_pending_cmds.end(); ) {
      auto xcmd = (*itr);
      auto exec = xcmd->get_exec();
      if (exec->get_scheduler()==this) {
        XRT_DEBUGF("xcmd(%d) [new->queued]\n",xcmd->get_uid());
        itr = s_pending_cmds.erase(itr);
        xcmd->set_int_state(ERT_CMD_STATE_QUEUED);
        m_command_queue.push_back(std::move(xcmd));
      }
      else {
        ++itr;
      }
    }
    s_num_pending = s_pending_cmds.size();
  }

  // Transition command to submitted state if possible
  bool
  queued_to_submitted(const xcmd_ptr& xcmd)
  {
    bool retval = false;
    auto exec = xcmd->get_exec();
    if (exec->submit(xcmd.get())) {
      XRT_DEBUGF("xcmd(%d) [queued->submitted]\n",xcmd->get_uid());
      xcmd->set_int_state(ERT_CMD_STATE_SUBMITTED);
      retval = true;
    }
    return retval;
  }

  // Transition command to running state if possible
  bool
  submitted_to_running(const xcmd_ptr& xcmd)
  {
    bool retval = false;
    auto exec = xcmd->get_exec();
    if (exec->start(xcmd.get())) {
      XRT_DEBUGF("xcmd(%d) [submitted->running]\n",xcmd->get_uid());
      xcmd->set_int_state(ERT_CMD_STATE_RUNNING);
      retval = true;
    }
    return retval;
  }

  // Transition command to complete state if command has completed
  bool
  running_to_complete(const xcmd_ptr& xcmd)
  {
    bool retval = false;
    auto exec = xcmd->get_exec();
    if (exec->query(xcmd.get())) {
      XRT_DEBUGF("xcmd(%d) [running->complete]\n",xcmd->get_uid());
      xcmd->set_state(ERT_CMD_STATE_COMPLETED);
      xcmd->notify_host();
      retval = true;
    }
    return retval;
  }

  // Free a command
  bool
  complete_to_free(const xcmd_ptr& xcmd)
  {
    XRT_DEBUGF("xcmd(%d) [complete->free]\n",xcmd->get_uid());
    return true;
  }

  // Iterate command queue and baby sit all commands
  void
  iterate_cmds()
  {
    auto end = m_command_queue.end();
    auto nitr = m_command_queue.begin();
    for (auto itr=nitr; itr!=end; itr=nitr) {
      auto& xcmd = (*itr);
      if (xcmd->get_state() == ERT_CMD_STATE_QUEUED)
        queued_to_submitted(xcmd);
      if (xcmd->get_state() == ERT_CMD_STATE_SUBMITTED)
        submitted_to_running(xcmd);
      if (xcmd->get_state() == ERT_CMD_STATE_RUNNING)
        running_to_complete(xcmd);
      if (xcmd->get_state() == ERT_CMD_STATE_COMPLETED) {
        complete_to_free(xcmd);
        nitr = m_command_queue.erase(itr);
        end = m_command_queue.end();
        continue;
      }

      nitr = ++itr;
    }
  }

  // Wait until something interesting happens
  void
  wait()
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (!m_stop && !s_num_pending && m_command_queue.empty())
      m_work.wait(lk);

    if (m_stop) {
      if (!m_command_queue.empty() || s_num_pending)
        throw std::runtime_error("software scheduler stopping while there are active commands");
    }
  }


  // Loop once
  void
  loop()
  {
    wait();
    queue_cmds();
    iterate_cmds();
  }

public:

  // Wake up the scheduler if it is waiting
  void
  notify()
  {
    m_work.notify_one();
  }

  // Run the scheduler until it is stopped
  void
  run()
  {
    while (!m_stop)
      loop();
  }

  // Stop the scheduler
  void
  stop()
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_stop = true;
    m_work.notify_one();
  }

};

////////////////////////////////////////////////////////////////
// One static scheduler currently on a single thread
static xocl_scheduler s_global_scheduler;
static std::thread s_scheduler_thread;
static bool s_running=false;

// Each device has a execution core
static std::map<const xrt::device*, std::unique_ptr<exec_core>> s_device_exec_core;

// Thread routine for scheduler loop
static void
scheduler_loop()
{
  s_global_scheduler.run();
}

} // namespace

namespace xrt { namespace sws {

void
schedule(const cmd_ptr& cmd)
{
  auto device = cmd->get_device();

  auto& exec = s_device_exec_core[device];
  auto xcmd = xocl_cmd::create(exec.get(),cmd);
  auto scheduler = exec->get_scheduler();

  std::lock_guard<std::mutex> lk(s_pending_mutex);
  s_pending_cmds.push_back(xcmd);
  ++s_num_pending;
  scheduler->notify();
}

void
start()
{
  if (s_running)
    throw std::runtime_error("software command scheduler is already started");

  s_scheduler_thread = std::move(xrt::thread(scheduler_loop));
  if (threaded_notification)
    notifier = std::move(xrt::thread(xrt::task::worker,std::ref(notify_queue)));
  s_running = true;
}

void
stop()
{
  if (!s_running)
    return;

  s_global_scheduler.stop();
  s_scheduler_thread.join();

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
init(xrt::device* xdev, const std::vector<uint32_t>& cu_amap)
{
  // create execution core for this device
  auto slots = ERT_CQ_SIZE / xrt::config::get_ert_slotsize();
  s_device_exec_core.erase(xdev);
  s_device_exec_core.insert
    (std::make_pair
     (xdev,std::make_unique<exec_core>(xdev,&s_global_scheduler,slots,cu_amap)));
}

}} // sws,xrt
