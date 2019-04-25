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

int
command::
execute()
{
  return m_impl->execute();
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

exec_write_command::
exec_write_command(xrt_device* device)
  : command(device,ERT_EXEC_WRITE)
{
  m_impl->ecmd->type = ERT_CU;
  m_impl->ecmd->count = 1+4; // cumask + 4 ctrl
}

void
exec_write_command::
add_cu(value_type cuidx)
{
  if (cuidx>=32)
    throw std::runtime_error("write_command supports at most 32 CUs");
  auto skcmd = reinterpret_cast<ert_start_kernel_cmd*>(m_impl->ecmd);
  skcmd->cu_mask |= 1<<cuidx;

  auto xdevice = m_impl->get_device();
  xdevice->acquire_cu_context(cuidx,true);
}

void
exec_write_command::
add(addr_type addr, value_type value)
{
  (*m_impl)[++m_impl->ecmd->count] = addr;
  (*m_impl)[++m_impl->ecmd->count] = value;
}

void
exec_write_command::
clear()
{
  // clear cu
  auto skcmd = reinterpret_cast<ert_start_kernel_cmd*>(m_impl->ecmd);
  skcmd->cu_mask = 0;

  // clear payload
  m_impl->ecmd->count = 1+4; // cumask + 4 ctrl
}

}} // exec,xrt
