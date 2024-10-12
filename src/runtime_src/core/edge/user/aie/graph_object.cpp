// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "graph_object.h"
#include "core/common/system.h"
#include <memory>
#include "core/edge/user/shim.h"

namespace zynqaie {
graph_object::graph_object(ZYNQ::shim* shim, const xrt::uuid& uuid , const char* gname,
                xrt::graph::access_mode am, zynqaie::hwctx_object* hwctx)
  : m_shim{shim},
    name{gname},
    access_mode{am},
    m_hwctx{hwctx}
{
  auto device{xrt_core::get_userpf_device(m_shim)};
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

#ifdef XRT_ENABLE_AIE
  if (m_hwctx) {
    m_aie_array = m_hwctx->get_aie_array_shared();
    m_hwctx->get_aied()->register_graph(this);
  }
  else if (drv->isAieRegistered()) {
    m_aie_array = drv->get_aie_array_shared();
    drv->getAied()->register_graph(this);
  }
#endif

  id = xrt_core::edge::aie::get_graph_id(device.get(), name, m_hwctx);
  if (id == xrt_core::edge::aie::NON_EXIST_ID)
    throw xrt_core::error(-EINVAL, "Can not get id for Graph '" + name + "'");

  drv->open_graph_context(m_hwctx, uuid.get(), id, am);

  /* Initialize graph tile metadata */
  graph_config = xrt_core::edge::aie::get_graph(device.get(), name, m_hwctx);

  /* Initialize graph rtp metadata */
  rtps = xrt_core::edge::aie::get_rtp(device.get(), graph_config.id, m_hwctx);
  graph_api_obj = std::make_shared<adf::graph_api>(&graph_config, m_aie_array->get_config());
  graph_api_obj->configure();
  state = graph_state::reset;
}

graph_object::~graph_object()
{
  auto device{xrt_core::get_userpf_device(m_shim)};
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());
  drv->close_graph_context(m_hwctx, id);

  if (m_hwctx) // hwctx specific
    m_hwctx->get_aied()->deregister_graph(this);
  else   // device specific
    drv->getAied()->deregister_graph(this);    
}

std::string
graph_object::getname() const
{
  return name;
}

unsigned short
graph_object::getstatus() const
{
  return static_cast<unsigned short>(state);
}

void
graph_object::reset_graph()
{
  if (access_mode == xrt::graph::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared context can not reset graph");

  for (int i = 0; i < graph_config.coreColumns.size(); i++)
  {
    XAie_LocType coreTile = XAie_TileLoc(graph_config.coreColumns[i], graph_config.coreRows[i] + m_aie_array->get_config()->get_num_reserved_rows() + 1);
    XAie_CoreDisable(m_aie_array->get_dev(), coreTile);
  }

  state = graph_state::reset;
}

uint64_t
graph_object::get_timestamp()
{
  /* TODO just use the first tile to get the timestamp? */
  XAie_LocType coreTile = XAie_TileLoc(graph_config.coreColumns[0], graph_config.coreRows[0] + m_aie_array->get_config()->get_num_reserved_rows() + 1);
  uint64_t timeStamp;
  AieRC rc = XAie_ReadTimer(m_aie_array->get_dev(), coreTile, XAIE_CORE_MOD, &timeStamp);
  if (rc != XAIE_OK)
    throw xrt_core::error(-EINVAL, "Fail to read timestamp for Graph '" + name);

  return timeStamp;
}

void
graph_object::run_graph(int iterations)
{
  if (iterations == 0)
  {
    if (access_mode == xrt::graph::access_mode::shared)
      throw xrt_core::error(-EPERM, "Shared context can not run graph");

    if (state != graph_state::stop && state != graph_state::reset)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is already running or has ended");

    graph_api_obj->run();
    state = graph_state::running;
  }
  else
  {
    if (access_mode == xrt::graph::access_mode::shared)
      throw xrt_core::error(-EPERM, "Shared context can not run graph");

    if (state != graph_state::stop && state != graph_state::reset)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is already running or has ended");

    graph_api_obj->run(iterations);
    state = graph_state::running;
  }
}

int
graph_object::wait_graph_done(int timeout_ms)
{
  if (access_mode == xrt::graph::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared context can not wait on graph");

  if (state == graph_state::stop)
    return 0;

  if (state != graph_state::running)
    throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot wait");

  auto begin = std::chrono::high_resolution_clock::now();

  /*
   * We are using busy waiting here. Until every tile in the graph
   * is done, we keep polling each tile.
   */
  while (1)
  {
    uint8_t done;
    for (int i = 0; i < graph_config.coreColumns.size(); i++)
    {
      /* Skip multi-rate core */
      if (graph_config.triggered[i])
      {
        done = 1;
        continue;
      }

      XAie_LocType coreTile = XAie_TileLoc(graph_config.coreColumns[i], graph_config.coreRows[i] + m_aie_array->get_config()->get_num_reserved_rows() + 1);
      XAie_CoreReadDoneBit(m_aie_array->get_dev(), coreTile, &done);
      if (!done)
        break;
    }
    if (done)
    {
      state = graph_state::stop;
      for (int i = 0; i < graph_config.coreColumns.size(); i++)
      {
        if (graph_config.triggered[i])
          continue;

	XAie_LocType coreTile = XAie_TileLoc(graph_config.coreColumns[i], graph_config.coreRows[i] + m_aie_array->get_config()->get_num_reserved_rows() + 1);
	XAie_CoreDisable(m_aie_array->get_dev(), coreTile);
      }
      return 0;
    }

    auto current = std::chrono::high_resolution_clock::now();
    auto dur = current - begin;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    if (timeout_ms >= 0 && timeout_ms < ms)
      throw xrt_core::error(-ETIME, "Wait graph '" + name + "' timeout.");
  }
  return -1;
}

void
graph_object::wait_graph(uint64_t cycle)
{
  if (cycle == 0)
  {
    if (access_mode == xrt::graph::access_mode::shared)
      throw xrt_core::error(-EPERM, "Shared context can not wait on graph");

    if (state == graph_state::stop)
      return;

    if (state != graph_state::running)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot wait");

    graph_api_obj->wait();
    state = graph_state::stop;
  }
  else
  {
    if (access_mode == xrt::graph::access_mode::shared)
      throw xrt_core::error(-EPERM, "Shared context can not wait on graph");

    if (state == graph_state::suspend)
      return;

    if (state != graph_state::running)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot wait");

    graph_api_obj->wait(cycle);
    state = graph_state::suspend;
  }
}

void
graph_object::suspend_graph()
{
  if (access_mode == xrt::graph::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared context can not suspend graph");

  if (state != graph_state::running)
    throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot suspend");

  for (int i = 0; i < graph_config.coreColumns.size(); i++)
  {
    XAie_LocType coreTile = XAie_TileLoc(graph_config.coreColumns[i], graph_config.coreRows[i] + m_aie_array->get_config()->get_num_reserved_rows() + 1);
    XAie_CoreDisable(m_aie_array->get_dev(), coreTile);
  }
  state = graph_state::suspend;
}

void
graph_object::resume_graph()
{
  if (access_mode == xrt::graph::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared context can not resume on graph");

  if (state != graph_state::suspend)
    throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not suspended (wait(cycle)), cannot resume");

  graph_api_obj->resume();
  state = graph_state::running;
}

void
graph_object::end_graph(uint64_t cycle)
{
  if (cycle == 0)
  {
    if (access_mode == xrt::graph::access_mode::shared)
      throw xrt_core::error(-EPERM, "Shared context can not end graph");

    if (state != graph_state::running && state != graph_state::stop)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running or stop, cannot end");

    graph_api_obj->end();
    state = graph_state::end;
  }
  else
  {
    if (access_mode == xrt::graph::access_mode::shared)
      throw xrt_core::error(-EPERM, "Shared context can not end graph");

    if (state != graph_state::running && state != graph_state::suspend)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running or suspended, cannot end(cycle_timeout)");

    graph_api_obj->end(cycle);
    state = graph_state::end;
  }
}

void
graph_object::update_graph_rtp(const char* port, const char* buffer, size_t size)
{
  auto it = rtps.find(port);
  if (it == rtps.end())
    throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': RTP port '" + port + "' not found");
  auto& rtp = it->second;

  if (access_mode == xrt::graph::access_mode::shared && !rtp.isAsync)
    throw xrt_core::error(-EPERM, "Shared context can not update sync RTP");

  if (rtp.isPL)
    throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': RTP port '" + port + "' is not AIE RTP");

  graph_api_obj->update(&rtp, (const void*)buffer, size);
}

void
graph_object::read_graph_rtp(const char* port, char* buffer, size_t size)
{
  auto it = rtps.find(port);
  if (it == rtps.end())
    throw xrt_core::error(-EINVAL, "Can't read graph '" + name + "': RTP port '" + port + "' not found");
  auto& rtp = it->second;

  if (rtp.isPL)
    throw xrt_core::error(-EINVAL, "Can't read graph '" + name + "': RTP port '" + port + "' is not AIE RTP");

  graph_api_obj->read(&rtp, (void*)buffer, size);
}
}
