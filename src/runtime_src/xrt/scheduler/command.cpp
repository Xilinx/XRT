/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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

#include "command.h"
#include "scheduler.h"

#include <map>
#include <vector>

namespace {

using buffer_type = xrt::device::ExecBufferObjectHandle;
static std::mutex s_mutex;

// Static destruction logic to prevent double purging.

// Exec buffer objects must be purged before device is closed.  Static
// destruction calls platform dtor, which in turns calls purge
// commands, but static destruction could have deleted the static
// object in this file first.
static bool s_purged = false;

struct X {
  std::map<xrt::device*,std::vector<buffer_type>> freelist;
  X() {}
  ~X() { s_purged = true; }
};

static X sx;

static buffer_type
get_buffer(xrt::device* device,size_t sz)
{
  std::lock_guard<std::mutex> lk(s_mutex);

  auto itr = sx.freelist.find(device);
  if (itr != sx.freelist.end()) {
    auto& freelist = (*itr).second;
    if (!freelist.empty()) {
      auto buffer = freelist.back();
      freelist.pop_back();
      return buffer;
    }
  }

  return device->allocExecBuffer(sz); // not thread safe
}

static void
free_buffer(xrt::device* device,buffer_type bo)
{
  std::lock_guard<std::mutex> lk(s_mutex);
  s_purged=false;
  sx.freelist[device].emplace_back(std::move(bo));
}

} // namespace

namespace xrt {

// Purge exec buffer freelist during static destruction.
// Not safe to call outside of static descruction, can't lock
// static mutex since it could have been destructed
void
purge_command_freelist()
{
  if (s_purged)
    return;

  for (auto& elem : sx.freelist)
    elem.second.clear();

  s_purged = true;
}

// Purge command list for specific device
// This function is called when the device is closed
void
purge_device_command_freelist(xrt::device* device)
{
  auto itr = sx.freelist.find(device);
  if (itr != sx.freelist.end())
    (*itr).second.clear();
}

command::
command(xrt::device* device, ert_cmd_opcode opcode)
  : m_device(device)
  , m_exec_bo(get_buffer(m_device,regmap_size*sizeof(value_type)))
  , m_packet(m_device->map(m_exec_bo))
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  // Clear in case packet was recycled
  m_packet.clear();

  auto epacket = get_ert_cmd<ert_packet*>();
  epacket->state = ERT_CMD_STATE_NEW; // new command
  epacket->opcode = opcode & 0x1F; // [4:0]
  epacket->type = opcode >> 5;     // [9:5]

  XRT_DEBUG(std::cout,"xrt::command::command(",m_uid,")\n");

}

command::
command(command&& rhs)
  : m_uid(rhs.m_uid), m_device(rhs.m_device)
  , m_exec_bo(std::move(rhs.m_exec_bo))
  , m_packet(std::move(rhs.m_packet))
{
  rhs.m_exec_bo = 0;
}

command::
~command()
{
  if (m_exec_bo) {
    XRT_DEBUG(std::cout,"xrt::command::~command(",m_uid,")\n");
    m_device->unmap(m_exec_bo);
    free_buffer(m_device,m_exec_bo);
  }
}

void
command::
execute()
{
  // command objects can be reused outside constructor
  // reset state
  auto epacket = get_ert_cmd<ert_packet*>();
  epacket->state = ERT_CMD_STATE_NEW;

  m_done=false;
  xrt::scheduler::schedule(get_ptr());
}

} // xrt
