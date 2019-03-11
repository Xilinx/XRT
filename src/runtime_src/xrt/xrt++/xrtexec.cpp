/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include "xrtexec.hpp"
#include "xrt/device/device.h"
#include "xrt/scheduler/command.h"
#include "xrt/scheduler/scheduler.h"

namespace xrtcpp {

namespace exec {

struct command::impl : xrt::command
{
  impl(xrt::device* device, ert_cmd_opcode opcode)
    : xrt::command(device,opcode)
      //    , ecmd(get_ert_cmd<ert_packet*>())
  {
    ecmd = get_ert_cmd<ert_packet*>();
  }

  ert_packet* ecmd = nullptr;
};

command::
command(xrt_device* device, ert_cmd_opcode opcode)
  : m_impl(std::make_shared<impl>(static_cast<xrt::device*>(device),opcode))
{}

void
command::
execute()
{
  xrt::scheduler::schedule(m_impl);
}

void
command::
wait()
{
  m_impl->wait();
}

bool
command::
completed() const
{
  return m_impl->completed();
}

write_command::
write_command(xrt_device* device)
  : command(device,ERT_WRITE)
{
  m_impl->ecmd->type = ERT_KDS_LOCAL;
}

void
write_command::
add(addr_type addr, value_type value)
{
  (*m_impl)[offset++] = addr;
  (*m_impl)[offset++] = value;
  m_impl->ecmd->count += 2;
}

}} // exec,xrt
