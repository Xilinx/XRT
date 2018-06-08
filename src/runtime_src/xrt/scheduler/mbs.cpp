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

//#define MBS_VERBOSE

#if defined(MBS_VERBOSE) && !defined(XRT_VERBOSE)
# define XRT_VERBOSE
#endif


/**
 * XRT MB command scheduler (when using embedded MB CU scheduler)
 */
#include "xrt/config.h"
#include "xrt/util/error.h"
#include "xrt/util/thread.h"
#include "xrt/util/debug.h"
#include "xrt/util/time.h"
#include "xrt/util/task.h"
#include "command.h"

#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>
#include <vector>
#include <bitset>
#include <list>
#include <map>
#include <sstream>
#include <cassert>
#include <iostream>

namespace {

////////////////////////////////////////////////////////////////
// Convenience types for readability
////////////////////////////////////////////////////////////////
using size_type = uint32_t;
using addr_type = uint32_t;
using value_type = uint32_t;
using command_type = std::shared_ptr<xrt::command>;
using command_queue_type = std::list<command_type>;
//using command_queue_type = std::vector<command_type>;

////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////
const size_type max_cus = 128;
using bitmask_type = std::bitset<max_cus>;

XRT_UNUSED
std::string
to_hex(size_t sz)
{
  std::stringstream stream;
  stream << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << sz;
  return stream.str();
}

XRT_UNUSED
static std::string
byte_to_binary(uint8_t byte)
{
  char b[9] = {0};
  for (int z = 128; z > 0; z >>= 1)
    strcat(b, ((byte & z) == z) ? "1" : "0");
  return b;
}

// Debugging status register read
XRT_UNUSED
static std::string
format_status_data(const uint8_t* data,size_t bytes)
{
  static size_t count = 0;
  ++count;
  if (std::none_of(data,data+bytes,[](uint8_t byte){return byte!=0;}))
    return "";

  std::stringstream stream;
  stream << "xrt::mbs status register after " << count << " polls ";
  count = 0;

  for (int i=bytes-1; i>=0; --i) {
    size_t byte = data[i];
    stream << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << byte;
  }
  stream << " : ";
  for (int i=bytes-1; i>=0; --i) {
    auto byte = data[i];
    stream << byte_to_binary(byte) << " ";
  }
  auto str = stream.str();
  XRT_PRINT(std::cout,str,"\n");
  return str;
}

inline size_t 
emulate_hw()
{
  static bool em_hw = std::getenv("MBS_EMULATE_HW") != nullptr;
  return em_hw;
}

inline void
write_register(xrt::device* dev, size_t offset, const void* buffer, size_t size)
{
  if (!emulate_hw())
    dev->write_register(offset,buffer,size);
}

inline void
read_register(xrt::device* dev, size_t offset, void* buffer, size_t size)
{
  if (!emulate_hw())
    dev->read_register(offset,buffer,size);
  else if (std::rand()%5==0) {
    auto data = static_cast<char*>(buffer);
    for (size_t i=0; i<size; ++i) {
      auto val = std::rand()%256;
      std::memcpy(data+i,&val,1);
    }
  }
}

////////////////////////////////////////////////////////////////
// Command notification is threaded through task queue
// and notifier.  This allows the scheduler to continue
// while host callback can be processed in the background
////////////////////////////////////////////////////////////////
static xrt::task::queue notify_queue;
static std::thread notifier;
static bool threaded_notification = true;

////////////////////////////////////////////////////////////////
// This mirrors a hardware command queue on the software side.
// Functionality to be subsumed by kernel driver
////////////////////////////////////////////////////////////////
namespace hardware {

const size_type CQ_SIZE                    = ERT_CQ_SIZE;
const addr_type CQ_ADDR                    = ERT_CQ_BASE_ADDR;
const addr_type CSR_ADDR                   = ERT_CSR_ADDR;
const addr_type CQ_COMPLETION_ADDR         = ERT_CSR_ADDR;
const addr_type CQ_STATUS_REGISTER_ADDR[4] = 
{
  ERT_CQ_STATUS_REGISTER_ADDR0
  ,ERT_CQ_STATUS_REGISTER_ADDR1
  ,ERT_CQ_STATUS_REGISTER_ADDR2
  ,ERT_CQ_STATUS_REGISTER_ADDR3
};

const size_type max_slots = 256;
const size_type no_index = xrt::command::no_index;

class queue
{
private:
  std::bitset<max_slots> m_used;
  std::bitset<max_slots> m_complete;

  xrt::device* m_device;
  const size_type m_slots;

  size_type
  slot_size() const
  {
    return CQ_SIZE / m_slots;
  }

  void
  read_status()
  {
    const size_type bytes = std::max(m_slots/8,static_cast<unsigned int>(4)); // minimum read 4 bytes
    uint8_t data[bytes] = {0};

    // throttle polling of status queue, otherwise it gets flodded with
    // reads and ert can't write to it, resulting in hang
    //std::this_thread::sleep_for(std::chrono::nanoseconds(50));

    read_register(m_device,CQ_COMPLETION_ADDR,&data,bytes);

    size_t idx = 0;
    for (auto byte : data) {
      // process all bits of byte if byte is non zero
      if (byte) {
        for (size_t b=0; b<8; ++b,++idx)
          if ( (byte>>b) & 1 )
            m_complete.set(idx);
      }
      // skiping byte, advance idx 8 bits
      else
        idx += 8;
    }

    // For sanity, clear any bits after m_max (shouldn't be set)
    for (auto idx=m_slots; idx<max_slots; ++idx)
      m_complete.reset(idx);
    // Also clear any bits not used (emulate_hw)
    if (emulate_hw())
      m_complete &= m_used;

#ifdef XRT_VERBOSE
    format_status_data(data,m_slots/8);
    //XRT_PRINT(std::cout,"used1:",m_used.to_string().substr(max_slots-m_slots),"\n");
    //XRT_PRINT(std::cout,"comp1:",m_complete.to_string().substr(max_slots-m_slots),"\n");
#endif

  }

public:
  
  queue(xrt::device* dev, size_type slot_size) : m_device(dev), m_slots(std::min(CQ_SIZE/slot_size,max_slots))
  {
    XRT_DEBUG(std::cout,"mbs cq slots(",m_slots,")\n");
    assert(m_slots<=max_slots);
    for (size_type i=m_slots; i<max_slots; ++i)
      m_used.set(i);
  }

  queue(queue&& rhs)
    : m_used(std::move(rhs.m_used))
    , m_complete(std::move(rhs.m_complete))
    , m_device(rhs.m_device)
    , m_slots(rhs.m_slots)
  {}

  xrt::device*
  get_device() const
  {
    return m_device;
  }

  size_type
  get_and_set_unused_index()
  {
    if (m_used.all())
      return no_index;
    size_type idx=0;
    while (idx<m_slots-1 && m_used.test(idx))
      ++idx;
    m_used.set(idx);
    return idx;
  }

  bool
  clear_if_ready(size_type idx)
  {
    if (!m_used.test(idx))
      return false;

    if (!m_complete.test(idx))
      read_status();

    if (m_complete.test(idx)) {
      m_used.reset(idx);
      m_complete.reset(idx);
#ifdef XRT_VERBOSE
      XRT_PRINT(std::cout,"used2:",m_used.to_string().substr(max_slots-m_slots),"\n");
      XRT_PRINT(std::cout,"comp2:",m_complete.to_string().substr(max_slots-m_slots),"\n");
#endif
      return true;
    }

    return false;
  }

  void
  write_command(const command_type& cmd)
  {
    auto slot_idx = cmd->slot_index;
    auto& packet = cmd->get_packet();
    XRT_DEBUG(std::cout,"xrt::mbs slot(",slot_idx,") writing ",packet.bytes()," bytes\n");
    auto header_offset = CQ_ADDR + slot_size()*slot_idx;
    auto data_offset = header_offset + sizeof(xrt::command::value_type); // past header


    auto header = packet[0];
    auto data = packet.data() + 1;
    auto data_bytes = packet.bytes() - sizeof(xrt::command::value_type);

    // write data first
    write_register(m_device,data_offset,data,data_bytes);

    // write header, start 0001
    auto epacket = cmd->get_ert_packet();
    epacket->state = 1;
    write_register(m_device,header_offset,&header,sizeof(xrt::command::value_type));

    // if cq slot interrupt -> mb enabled then write command slot
    // index to cqint register
    if (xrt::config::get_ert_cqint()) {
      size_type mask_idx = slot_idx/32;
      size_type mask = 1 << (slot_idx-mask_idx*32);
      XRT_DEBUGF("slot idx: %d mask_idx: %d mask: 0x%x",slot_idx,mask_idx,mask);
      write_register(m_device,CQ_STATUS_REGISTER_ADDR[mask_idx],&mask,sizeof(size_type));
    }
  }

}; // class queue
  
using d2q_type = std::map<const xrt::device*, queue>;
static d2q_type d2q;

queue*
get_queue(const xrt::device* device)
{
  auto itr = d2q.find(device);
  return (itr!=d2q.end())
    ? &(itr->second)
    : nullptr;
}

queue*
get_queue_or_error(const xrt::device* device)
{
  auto itr = d2q.find(device);
  if (itr==d2q.end())
    throw std::runtime_error("No hardware command queue for device");

  return &(itr->second);
}

void
add_queue(xrt::device* device, size_type slot_size)
{
  if (get_queue(device))
    return;

  queue q(device,slot_size);
  d2q.emplace(device,std::move(q));
}

} // hardware

size_type
get_command_index(const xrt::device* device)
{
  auto queue = hardware::get_queue_or_error(device);
  return queue->get_and_set_unused_index();
}

bool
is_command_done(const command_type& cmd)
{
  auto queue = hardware::get_queue_or_error(cmd->get_device());
  return queue->clear_if_ready(cmd->slot_index);
}

void
write(const command_type& cmd)
{
  auto queue = hardware::get_queue_or_error(cmd->get_device());
  queue->write_command(cmd);
}


////////////////////////////////////////////////////////////////
// Main command scheduler interfacing to embedded MB scheduler
////////////////////////////////////////////////////////////////
static std::mutex s_mutex;
static std::condition_variable s_work;
static std::vector<command_type> s_cmds;
static bool s_running = false;
static bool s_stop = false;
static bool s_sleeping = false;
static std::exception_ptr s_exception;

static bool s_conformance_mode = (getenv("XCL_CONFORMANCE")!=nullptr);

static bool
start(const command_type& cmd)
{
  auto idx = cmd->slot_index = get_command_index(cmd->get_device());
  if (idx != xrt::command::no_index) {
    XRT_DEBUG(std::cout,"xrt::mbs::command(",cmd->get_uid(),") [pending->running]\n");
    cmd->state = xrt::command::state_type::running;
    write(cmd);
    return true;
  }
  return false;
}

static bool
check(const command_type& cmd)
{
  if (is_command_done(cmd)) {
    XRT_DEBUG(std::cout,"xrt::mbs::command(",cmd->get_uid(),") [running->done]\n");
    if (!threaded_notification) {
      cmd->state = xrt::command::state_type::done;
      cmd->done();
      return true;
    }

    auto notify = [](command_type cmd) {
      cmd->state = xrt::command::state_type::done;
      cmd->done();
    };

    xrt::task::createF(notify_queue,notify,cmd);
    return true;
  }
  return false;
}

// Babysit commands
static void
update(command_queue_type& cmds)
{
  auto end = cmds.end();
  for (auto itr=cmds.begin(); itr!=end; ) {
    auto& cmd = (*itr);
    switch (cmd->state) {
      case xrt::command::state_type::pending :
        start(cmd);
        ++itr;
        break;
      case xrt::command::state_type::running :
        if (check(cmd)) {
          itr = cmds.erase(itr);
          end = cmds.end();
        }
        else {
          ++itr;
        }
        break;
      case xrt::command::state_type::done :
        throw std::runtime_error("command::state::done not expected");
        break;
    }
  }
}

void 
launch(command_type cmd)
{
  XRT_DEBUG(std::cout,"xrt::mbs::command(",cmd->get_uid(),") [new->pending]\n");

  // lock the thread while starting cmd
  std::lock_guard<std::mutex> lk(s_mutex);

  // In conformance mode, let the scheduler start the first 
  // workgroup, it may have to reconfigure when all cus are idle
  // Otherwise, start the command here if scheduler is sleeping,
  // rather than relying on the scheduler to wake up and process
  // in update
  if (!s_conformance_mode && s_sleeping)
    start(cmd);

  s_cmds.emplace_back(std::move(cmd));
  s_work.notify_one();
}

static void
scheduler_loop()
{
  unsigned long loops = 0;           // number of outer loops
  unsigned long sleeps = 0;          // number of sleeps
  command_queue_type cmds;    // current set of command scheduler is managing

  while (1) {
    ++loops;

    {
      // Wake up when new commands are added.  Goal is to get out of
      // lk scope as quickly as possible thereby allowing new commands
      // to be added while existing ones are being monitored outside the lk
      std::unique_lock<std::mutex> lk(s_mutex);

      while (!s_stop && cmds.empty() && s_cmds.empty()) {
        ++sleeps;
        s_sleeping = true;  // optimization for launch
        s_work.wait(lk);
        s_sleeping = false; // optimization for launch
      }

      if (s_stop) {
        if (!s_cmds.empty() || !cmds.empty())
          throw std::runtime_error("command scheduler stopping while there are active commands");
        break;
      }
      
      // Save newly added commands
      std::copy(s_cmds.begin(),s_cmds.end(),std::back_inserter(cmds));
      s_cmds.clear();

    } // lk scope

    // throttle polling for cu completion
    if (auto us = xrt::config::get_polling_throttle())
      std::this_thread::sleep_for(std::chrono::microseconds(us));

    update(cmds);
  }
}

static void
scheduler()
{
  try {
    scheduler_loop();
  }
  catch (const std::exception& ex) { 
    std::string msg = std::string("mbs command scheduler died unexpectedly: ") + ex.what();
    xrt::send_exception_message(msg.c_str());
    s_exception = std::current_exception();
  }
  catch (...) {
    xrt::send_exception_message("mbs command scheduler died unexpectedly");
    s_exception = std::current_exception();
  }
}

static std::thread s_scheduler;

} // namespace


namespace xrt { namespace mbs {

void 
schedule(const command_type& cmd)
{
  launch(cmd);
}

void
start()
{
  if (s_running)
    throw std::runtime_error("mbs command scheduler is already started");

  std::lock_guard<std::mutex> lk(s_mutex);
  s_scheduler = std::move(xrt::thread(::scheduler));
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
  s_scheduler.join();

  notify_queue.stop();
  if (threaded_notification)
    notifier.join();

  s_running = false;
}

void
init(xrt::device* device, size_t regmap_size, bool cu_isr, size_t num_cus, size_t cu_offset, size_t cu_base_addr, const std::vector<uint32_t>& cu_addr_map)
{
  if (!xrt::config::get_ert())
    throw std::runtime_error("mbs scheduler called without ert enabled!");

  auto slot_size = xrt::config::get_ert_slotsize();
  hardware::add_queue(device,slot_size);

  auto cudma = xrt::config::get_ert_cudma();
  if (cudma && regmap_size>=0x210) // bug in cudma.c HW
    cudma = false;

  auto configure = std::make_shared<command>(device,command::opcode_type::configure);
  auto epacket = reinterpret_cast<ert_configure_cmd*>(configure->get_ert_packet());

  // variables (one word each)
  epacket->slot_size = slot_size;
  epacket->num_cus = num_cus;
  epacket->cu_shift = cu_offset;
  epacket->cu_base_addr = cu_base_addr;

  // features (one word) per sdaccel.ini
  epacket->ert     = xrt::config::get_ert();
  epacket->polling = 1; // only polling is supported in mbs mode
  epacket->cu_dma  = cudma;
  epacket->cu_isr  = cu_isr && xrt::config::get_ert_cuisr();
  epacket->cq_int  = xrt::config::get_ert_cqint();

  // cu addr map
  std::copy(cu_addr_map.begin(), cu_addr_map.end(), epacket->data);

  // payload size
  epacket->count = 5 + cu_addr_map.size();

  schedule(configure);

  // wait for command to complete
  XRT_PRINT(std::cout,"waiting for configure\n");
  while (configure->state!=command::state_type::done) ;
  XRT_PRINT(std::cout,"configure finished\n");
}

}} // mbs,xrt
