// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#include "xrtexec.hpp"
#include "xrt/device/device.h"
#include "core/common/bo_cache.h"
#include "core/common/api/command.h"
#include "core/common/api/hw_queue.h"

#include <functional>

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
  auto xdevice = static_cast<xrt_xocl::device*>(device);
  xdevice->acquire_cu_context(cuidx,true);
}

void
release_cu_context(xrt_device* device, value_type cuidx)
{
  auto xdevice = static_cast<xrt_xocl::device*>(device);
  xdevice->release_cu_context(cuidx);
}

xclDeviceHandle
get_device_handle(const xrt_device* device)
{
  auto xdevice = static_cast<const xrt_xocl::device*>(device);
  return xdevice->get_xcl_handle();
}


namespace exec {

using execbuf_type = xrt_core::bo_cache::cmd_bo<ert_packet>;
static std::mutex s_mutex;
static std::map<const xrt_xocl::device*, std::unique_ptr<xrt_core::bo_cache>> s_ebocache;

static execbuf_type
create_exec_buf(xrt_xocl::device* device)
{
  auto itr = s_ebocache.find(device);
  if (itr == s_ebocache.end()) {
    auto at_close = [] (const xrt_xocl::device* device) {
      s_ebocache.erase(device);
    };
    device->add_close_callback(std::bind(at_close, device));
    s_ebocache.emplace(device, std::make_unique<xrt_core::bo_cache>(device->get_xcl_handle(), 128));
  }

  return s_ebocache[device]->alloc<ert_packet>();
}

static void
release_exec_buf(const xrt_xocl::device* device, execbuf_type& ebo)
{
  s_ebocache[device]->release(ebo);
}

struct command::impl : xrt_core::command
{
  impl(xrt_xocl::device* device, ert_cmd_opcode opcode)
    : m_device(device)
    , m_hwqueue(device->get_core_device().get())
    , m_execbuf(create_exec_buf(m_device))
    , ert_pkt(reinterpret_cast<ert_packet*>(m_execbuf.second))
  {
    ert_pkt->state = ERT_CMD_STATE_NEW;
    ert_pkt->opcode = opcode & 0x1F; // [4:0]
  }

  ~impl()
  {
    release_exec_buf(m_device, m_execbuf);
  }

  xrt_xocl::device* m_device;
  xrt_core::hw_queue m_hwqueue;
  execbuf_type m_execbuf;      // underlying execution buffer
  mutable bool m_done = true;

  mutable std::mutex m_mutex;
  mutable std::condition_variable m_exec_done;

  union {
    uint32_t* data;
    ert_packet* ert_pkt;
    ert_start_kernel_cmd* ert_cu;
  };

  uint32_t
  operator[] (int idx) const
  {
    return data[idx];
  }

  uint32_t&
  operator[] (int idx)
  {
    return data[idx];
  }

  void
  run()
  {
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      if (!m_done)
        throw std::runtime_error("bad command state, can't launch");
      m_done = false;
    }

    m_hwqueue.unmanaged_start(this);
  }

  ert_cmd_state
  wait() const
  {
    m_hwqueue.wait(this);
    return static_cast<ert_cmd_state>(ert_pkt->state);
  }

  bool
  completed() const
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_done)
      return true;

    return (m_done = (ert_pkt->state >= ERT_CMD_STATE_COMPLETED));
  }

  ////////////////////////////////////////////////////////////////
  // Implement xrt_core::command API
  ////////////////////////////////////////////////////////////////
  virtual ert_packet*
  get_ert_packet() const
  {
    return ert_pkt;
  }

  virtual xrt_core::device*
  get_device() const
  {
    return m_device->get_core_device().get();
  }

  virtual xrt_buffer_handle
  get_exec_bo() const
  {
    return m_execbuf.first;
  }

  virtual xrt_hwctx_handle
  get_hwctx_handle() const
  {
      return XRT_NULL_HWCTX;
  }

  virtual void
  notify(ert_cmd_state s) const
  {
    if (s < ERT_CMD_STATE_COMPLETED)
      return;

    std::lock_guard<std::mutex> lk(m_mutex);
    m_done = true;
  }

};

command::
command(xrt_device* device, ert_cmd_opcode opcode)
  : m_impl(std::make_shared<impl>(static_cast<xrt_xocl::device*>(device),opcode))
{}

void
command::
execute()
{
  m_impl->ert_pkt->state = ERT_CMD_STATE_NEW;
  m_impl->run();
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
  : command(device,ERT_START_KEY_VAL)
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

  m_impl->ert_pkt->count = 1;
}

}} // exec,xrt
