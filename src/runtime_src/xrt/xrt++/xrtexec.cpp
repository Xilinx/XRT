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

inline index_type
mask_idx(index_type idx)
{
  return idx >> 5;
}

inline index_type
idx_in_mask(index_type idx)
{
  return idx - (mask_idx(idx) << 5);
}

static void
add_cu(ert_start_kernel_cmd* skcmd, index_type cuidx)
{
  auto maskidx = mask_idx(cuidx);
  if (maskidx > 3) 
    throw std::runtime_error("Bad CU idx : " + std::to_string(cuidx));

  // Shift payload down if necessary to make room for extra cu mask(s).
  // Current count includes mandatory cumask, which for skcmd is outside
  // the data[] array that is shifted here.
  if (maskidx > 0 && skcmd->extra_cu_masks < maskidx) {
    std::memmove(skcmd->data+maskidx,                  // dst
                 skcmd->data+skcmd->extra_cu_masks,    // src
                 (skcmd->count-1) * sizeof(uint32_t)); // exclude mandatory cumask
    skcmd->extra_cu_masks = maskidx;
    skcmd->count += maskidx;
  }

  // adjust for mandatory mask with index 0 is outside of skcmd->data
  auto& cumask = maskidx > 0 ? skcmd->data[maskidx-1] : skcmd->cu_mask;
  cumask |= 1 << idx_in_mask(cuidx);
}

void
acquire_cu_context(xrt_device* device, value_type cuidx)
{
  auto xdevice = static_cast<xrt::device*>(device);
  xdevice->acquire_cu_context(cuidx,true);
}

void
release_cu_context(xrt_device* device, value_type cuidx)
{
  auto xdevice = static_cast<xrt::device*>(device);
  xdevice->release_cu_context(cuidx);
}

xrt::device::device_handle
get_device_handle(const xrt_device* device)
{
  auto xdevice = static_cast<const xrt::device*>(device);
  return xdevice->get_handle();
}

namespace exec {

struct command::impl : xrt::command
{
  impl(xrt::device* device, ert_cmd_opcode opcode)
    : xrt::command(device,opcode)
      //    , ecmd(get_ert_cmd<ert_packet*>())
  {
    ert_pkt = get_ert_cmd<ert_packet*>();
  }

  union {
    ert_packet* ert_pkt;
    ert_start_kernel_cmd* ert_cu;
  };
};

command::
command(xrt_device* device, ert_cmd_opcode opcode)
  : m_impl(std::make_shared<impl>(static_cast<xrt::device*>(device),opcode))
{}

void
command::
execute()
{
  m_impl->execute();
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

ert_cmd_state
command::
state() const
{
  return static_cast<ert_cmd_state>(m_impl->ert_pkt->state);
}

exec_cu_command::
exec_cu_command(xrt_device* device)
  : command(device,ERT_START_CU)
{
  m_impl->ert_pkt->type = ERT_CU;
  clear();
}

void
exec_cu_command::
clear()
{
  // clear cu
  auto skcmd = m_impl->ert_cu;
  skcmd->cu_mask = 0;

  // clear payload since this command type is random write
  std::memset(m_impl->ert_pkt->data,0,m_impl->ert_pkt->count);

  // clear payload count
  m_impl->ert_pkt->count = 1 + 4; // cumask + 4 ctrl
}

void
exec_cu_command::
add_cu(value_type cuidx)
{
  xrtcpp::add_cu(m_impl->ert_cu,cuidx);
}

void
exec_cu_command::
add(index_type idx, value_type value)
{
  const auto skip = 1 + 1 + m_impl->ert_cu->extra_cu_masks; // header, cumask, extra cu
  (*m_impl)[skip+idx] = value;
  m_impl->ert_pkt->count = std::max(m_impl->ert_pkt->count,skip+idx);
}

exec_write_command::
exec_write_command(xrt_device* device)
  : command(device,ERT_EXEC_WRITE)
{
  m_impl->ert_pkt->type = ERT_CU;
  clear();
}

void
exec_write_command::
add_cu(value_type cuidx)
{
  xrtcpp::add_cu(m_impl->ert_cu,cuidx);
}

void
exec_write_command::
add_ctx(uint32_t ctx)
{
  if (ctx >= 32)
    throw std::runtime_error("write_exec supports at most 32 contexts numbered 0 through 31");
  
  auto skcmd = m_impl->ert_cu;
  skcmd->data[0x10 >> 2] = ctx;
}

void
exec_write_command::
add(addr_type addr, value_type value)
{
  (*m_impl)[++m_impl->ert_pkt->count] = addr;
  (*m_impl)[++m_impl->ert_pkt->count] = value;
}

void
exec_write_command::
clear()
{
  // clear cu
  auto skcmd = m_impl->ert_cu;
  skcmd->cu_mask = 0;

  // clear payload to past reserved fields. Reserved entries are
  // dictated by mandatory cumask and offset (0x10 and 0x14) of ctx-in
  // and ctx-out.  Since exec_write is piggybacking on start_kernel,
  // we have to skip past 4 control registers in register map also.
  // todo: create separate cmd packet for exec_write
  m_impl->ert_pkt->count = 1 + 4 + 2; 
}

}} // exec,xrt
