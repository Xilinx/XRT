/**
 * Copyright (C) 2020-2021 Xilinx, Inc
 * Author(s): Larry Liu
 * ZNYQ XRT Library layered on top of ZYNQ zocl kernel driver
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

#include "graph.h"
#ifndef __AIESIM__
#include "core/edge/user/shim.h"
#include "core/common/message.h"
#ifndef __HWEM__
#include "core/edge/user/plugin/xdp/aie_trace.h"
#include "core/edge/user/plugin/xdp/aie_profile.h"
#endif
#endif
#include "core/common/error.h"

#include <cstring>
#include <map>
#include <iostream>
#include <chrono>
#include <cerrno>

extern "C"
{
#include <xaiengine.h>
}

#ifdef __AIESIM__
zynqaie::Aie* getAieArray()
{
  static zynqaie::Aie s_aie(xrt_core::get_userpf_device(0));
  return &s_aie;
}
#endif

namespace zynqaie {

graph_type::
graph_type(std::shared_ptr<xrt_core::device> dev, const uuid_t uuid, const std::string& graph_name, xrt::graph::access_mode am)
  : device(std::move(dev)), name(graph_name)
{
#ifndef __AIESIM__
    auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

    if (!drv->isAieRegistered())
      throw xrt_core::error(-EINVAL, "No AIE presented");
    aieArray = drv->getAieArray();
#else
    aieArray = getAieArray();
#endif

#ifndef __AIESIM__
    id = xrt_core::edge::aie::get_graph_id(device.get(), name);
    if (id == xrt_core::edge::aie::NON_EXIST_ID)
        throw xrt_core::error(-EINVAL, "Can not get id for Graph '" + name + "'");

    int ret = drv->openGraphContext(uuid, id, am);
    if (ret)
        throw xrt_core::error(ret, "Can not open Graph context");
#endif
    access_mode = am;

    /* Initialize graph tile metadata */
    graph_config = xrt_core::edge::aie::get_graph(device.get(), name);

    /* Initialize graph rtp metadata */
    rtps = xrt_core::edge::aie::get_rtp(device.get(), graph_config.id);

    pAIEConfigAPI = std::make_shared<adf::graph_api>(&graph_config);
    pAIEConfigAPI->configure();

    state = graph_state::reset;
#ifndef __AIESIM__
    drv->getAied()->registerGraph(this);
#endif
}

graph_type::
~graph_type()
{
#ifndef __AIESIM__
    auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());
    drv->closeGraphContext(id);
    drv->getAied()->deregisterGraph(this);
#endif
}

std::string
graph_type::
getname() const
{
  return name;
}

unsigned short
graph_type::
getstatus() const
{
  return static_cast<unsigned short>(state);
}

void
graph_type::
reset()
{
    if (access_mode == xrt::graph::access_mode::shared)
        throw xrt_core::error(-EPERM, "Shared context can not reset graph");

    for (int i = 0; i < graph_config.coreColumns.size(); i++) {
      XAie_LocType coreTile = XAie_TileLoc(graph_config.coreColumns[i], graph_config.coreRows[i] + adf::config_manager::s_num_reserved_rows + 1);
      XAie_CoreDisable(aieArray->getDevInst(), coreTile);
    }

    state = graph_state::reset;
}

uint64_t
graph_type::
get_timestamp()
{
    /* TODO just use the first tile to get the timestamp? */
    XAie_LocType coreTile = XAie_TileLoc(graph_config.coreColumns[0], graph_config.coreRows[0] + adf::config_manager::s_num_reserved_rows + 1);

    uint64_t timeStamp;
    AieRC rc = XAie_ReadTimer(aieArray->getDevInst(), coreTile, XAIE_CORE_MOD, &timeStamp);
    if (rc != XAIE_OK)
        throw xrt_core::error(-EINVAL, "Fail to read timestamp for Graph '" + name);

    return timeStamp;
}

void
graph_type::
run()
{
    if (access_mode == xrt::graph::access_mode::shared)
        throw xrt_core::error(-EPERM, "Shared context can not run graph");

    if (state != graph_state::stop && state != graph_state::reset)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is already running or has ended");

    pAIEConfigAPI->run();

    state = graph_state::running;
}

void
graph_type::
run(int iterations)
{
    if (access_mode == xrt::graph::access_mode::shared)
        throw xrt_core::error(-EPERM, "Shared context can not run graph");

    if (state != graph_state::stop && state != graph_state::reset)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is already running or has ended");

    pAIEConfigAPI->run(iterations);

    state = graph_state::running;
}

void
graph_type::
wait_done(int timeout_ms)
{
    if (access_mode == xrt::graph::access_mode::shared)
        throw xrt_core::error(-EPERM, "Shared context can not wait on graph");

    if (state == graph_state::stop)
      return;

    if (state != graph_state::running)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot wait");

    auto begin = std::chrono::high_resolution_clock::now();

    /*
     * We are using busy waiting here. Until every tile in the graph
     * is done, we keep polling each tile.
     */
    while (1) {
        uint8_t done;
        for (int i = 0; i < graph_config.coreColumns.size(); i++){
            /* Skip multi-rate core */
            if (graph_config.triggered[i]) {
                done = 1;
                continue;
            }

            XAie_LocType coreTile = XAie_TileLoc(graph_config.coreColumns[i], graph_config.coreRows[i] + adf::config_manager::s_num_reserved_rows + 1);
            XAie_CoreReadDoneBit(aieArray->getDevInst(), coreTile, &done);
            if (!done)
                break;
        }

        if (done) {
            state = graph_state::stop;
            for (int i = 0; i < graph_config.coreColumns.size(); i++){
                if (graph_config.triggered[i])
                    continue;

                XAie_LocType coreTile = XAie_TileLoc(graph_config.coreColumns[i], graph_config.coreRows[i] + adf::config_manager::s_num_reserved_rows + 1);
                XAie_CoreDisable(aieArray->getDevInst(), coreTile);
            }
            return;
        }

        auto current = std::chrono::high_resolution_clock::now();
        auto dur = current - begin;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
        if (timeout_ms >= 0 && timeout_ms < ms)
          throw xrt_core::error(-ETIME, "Wait graph '" + name + "' timeout.");
    }
}

void
graph_type::
wait()
{
    if (access_mode == xrt::graph::access_mode::shared)
        throw xrt_core::error(-EPERM, "Shared context can not wait on graph");

    if (state == graph_state::stop)
        return;

    if (state != graph_state::running)
        throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot wait");

    pAIEConfigAPI->wait();

    state = graph_state::stop;
}

void
graph_type::
wait(uint64_t cycle)
{
    if (access_mode == xrt::graph::access_mode::shared)
        throw xrt_core::error(-EPERM, "Shared context can not wait on graph");

    if (state == graph_state::suspend)
        return;

    if (state != graph_state::running)
        throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot wait");

    pAIEConfigAPI->wait(cycle);

    state = graph_state::suspend;
}

void
graph_type::
suspend()
{
    if (access_mode == xrt::graph::access_mode::shared)
        throw xrt_core::error(-EPERM, "Shared context can not suspend graph");

    if (state != graph_state::running)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot suspend");

    for (int i = 0; i < graph_config.coreColumns.size(); i++) {
        XAie_LocType coreTile = XAie_TileLoc(graph_config.coreColumns[i], graph_config.coreRows[i] + adf::config_manager::s_num_reserved_rows + 1);
        XAie_CoreDisable(aieArray->getDevInst(), coreTile);
    }

    state = graph_state::suspend;
}

void
graph_type::
resume()
{
    if (access_mode == xrt::graph::access_mode::shared)
        throw xrt_core::error(-EPERM, "Shared context can not resume on graph");

    if (state != graph_state::suspend)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not suspended (wait(cycle)), cannot resume");

    pAIEConfigAPI->resume();

    state = graph_state::running;
}

void
graph_type::
end()
{
    if (access_mode == xrt::graph::access_mode::shared)
        throw xrt_core::error(-EPERM, "Shared context can not end graph");

    if (state != graph_state::running && state != graph_state::stop)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running or stop, cannot end");

    pAIEConfigAPI->end();

    state = graph_state::end;
}

void
graph_type::
end(uint64_t cycle)
{
    if (access_mode == xrt::graph::access_mode::shared)
        throw xrt_core::error(-EPERM, "Shared context can not end graph");

    if (state != graph_state::running && state != graph_state::suspend)
        throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running or suspended, cannot end(cycle_timeout)");

    pAIEConfigAPI->end(cycle);

    state = graph_state::end;
}


void
graph_type::
update_rtp(const std::string& port, const char* buffer, size_t size)
{
    auto it = rtps.find(port);
    if (it == rtps.end())
      throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': RTP port '" + port + "' not found");
    auto& rtp = it->second;

    if (access_mode == xrt::graph::access_mode::shared && !rtp.isAsync)
        throw xrt_core::error(-EPERM, "Shared context can not update sync RTP");

    if (rtp.isPL)
      throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': RTP port '" + port + "' is not AIE RTP");

    pAIEConfigAPI->update(&rtp, (const void*)buffer, size);
}

void
graph_type::
read_rtp(const std::string& port, char* buffer, size_t size)
{
    auto it = rtps.find(port);
    if (it == rtps.end())
      throw xrt_core::error(-EINVAL, "Can't read graph '" + name + "': RTP port '" + port + "' not found");
    auto& rtp = it->second;

    if (rtp.isPL)
      throw xrt_core::error(-EINVAL, "Can't read graph '" + name + "': RTP port '" + port + "' is not AIE RTP");

    pAIEConfigAPI->read(&rtp, (void*)buffer, size);
}

} // zynqaie

namespace {

using graph_type = zynqaie::graph_type;

// Active graphs per xrtGraphOpen/Close.  This is a mapping from
// xclGraphHandle to the corresponding graph object. xclGraphHandles
// is the address of the graph object.  This is shared ownership, as
// internals can use the graph object while applicaiton has closed the
// correspoding handle. The map content is deleted when user closes
// the handle, but underlying graph object may remain alive per
// reference count.
static std::map<xclGraphHandle, std::shared_ptr<graph_type>> graphs;

static std::shared_ptr<graph_type>
get_graph(xclGraphHandle ghdl)
{
  auto itr = graphs.find(ghdl);
  if (itr == graphs.end())
    throw std::runtime_error("Unknown graph handle");
  return (*itr).second;
}

}

namespace api {

using graph_type = zynqaie::graph_type;

static inline
std::string value_or_empty(const char* s)
{
  return s == nullptr ? "" : s;
}

xclGraphHandle
xclGraphOpen(xclDeviceHandle dhdl, const uuid_t xclbin_uuid, const char* name, xrt::graph::access_mode am)
{
  auto device = xrt_core::get_userpf_device(dhdl);
  auto graph = std::make_shared<graph_type>(device, xclbin_uuid, name, am);
  auto handle = graph.get();
  graphs.emplace(std::make_pair(handle,std::move(graph)));
  return handle;
}

void
xclGraphClose(xclGraphHandle ghdl)
{
  auto graph = get_graph(ghdl);
  graphs.erase(graph.get());
}

void
xclGraphReset(xclGraphHandle ghdl)
{
  auto graph = get_graph(ghdl);
  graph->reset();
}

uint64_t
xclGraphTimeStamp(xclGraphHandle ghdl)
{
  auto graph = get_graph(ghdl);
  return graph->get_timestamp();
}

void
xclGraphRun(xclGraphHandle ghdl, int iterations)
{
  auto graph = get_graph(ghdl);
  if (iterations == 0)
    graph->run();
  else
    graph->run(iterations);
}

void
xclGraphWaitDone(xclGraphHandle ghdl, int timeout_ms)
{
  auto graph = get_graph(ghdl);
  graph->wait_done(timeout_ms);
}

void
xclGraphWait(xclGraphHandle ghdl, uint64_t cycle)
{
  auto graph = get_graph(ghdl);
  if (cycle == 0)
    graph->wait();
  else
    graph->wait(cycle);
}

void
xclGraphSuspend(xclGraphHandle ghdl)
{
  auto graph = get_graph(ghdl);
  graph->suspend();
}

void
xclGraphResume(xclGraphHandle ghdl)
{
  auto graph = get_graph(ghdl);
  graph->resume();
}

void
xclGraphEnd(xclGraphHandle ghdl, uint64_t cycle)
{
  auto graph = get_graph(ghdl);
  if (cycle == 0)
    graph->end();
  else
    graph->end(cycle);
}

void
xclGraphUpdateRTP(xclGraphHandle ghdl, const char* port, const char* buffer, size_t size)
{
  auto graph = get_graph(ghdl);
  graph->update_rtp(port, buffer, size);
}

void
xclGraphReadRTP(xclGraphHandle ghdl, const char* port, char* buffer, size_t size)
{
  auto graph = get_graph(ghdl);
  graph->read_rtp(port, buffer, size);
}

void
xclSyncBOAIE(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
#ifndef __AIESIM__
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");
  auto aieArray = drv->getAieArray();
#else
  auto aieArray = getAieArray();
#endif

  auto bosize = bo.size();

  if (offset + size > bosize)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: exceed BO boundary.");

  aieArray->sync_bo(bo, gmioName, dir, size, offset);
}

void
xclSyncBOAIENB(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
#ifndef __AIESIM__
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");
  auto aieArray = drv->getAieArray();
#else
  auto aieArray = getAieArray();
#endif

  auto bosize = bo.size();

  if (offset + size > bosize)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: exceed BO boundary.");

  aieArray->sync_bo_nb(bo, gmioName, dir, size, offset);
}

void
xclGMIOWait(xclDeviceHandle handle, const char *gmioName)
{
#ifndef __AIESIM__
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");
  auto aieArray = drv->getAieArray();
#else
  auto aieArray = getAieArray();
#endif

  aieArray->wait_gmio(gmioName);
}

void
xclResetAieArray(xclDeviceHandle handle)
{
#ifndef __AIESIM__
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");
  auto aieArray = drv->getAieArray();
  aieArray->reset(device.get());
#else
  auto aieArray = getAieArray();
#endif
}

int
xclStartProfiling(xclDeviceHandle handle, int option, const char* port1Name, const char* port2Name, uint32_t value)
{
#ifndef __AIESIM__
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");

  auto aieArray = drv->getAieArray();
#else
  auto aieArray = getAieArray();
#endif

  return aieArray->start_profiling(option, value_or_empty(port1Name), value_or_empty(port2Name), value);
}

uint64_t
xclReadProfiling(xclDeviceHandle handle, int phdl)
{
#ifndef __AIESIM__
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");

  auto aieArray = drv->getAieArray();
#else
  auto aieArray = getAieArray();
#endif

  return aieArray->read_profiling(phdl);
}

void
xclStopProfiling(xclDeviceHandle handle, int phdl)
{
#ifndef __AIESIM__
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");

  auto aieArray = drv->getAieArray();
#else
  auto aieArray = getAieArray();
#endif

  return aieArray->stop_profiling(phdl);
}

} // api


////////////////////////////////////////////////////////////////
// Shim level Graph API implementations (xcl_graph.h)
////////////////////////////////////////////////////////////////
xclGraphHandle
xclGraphOpen(xclDeviceHandle handle, const uuid_t xclbin_uuid, const char* graph, xrt::graph::access_mode am)
{
  try {
    return api::xclGraphOpen(handle, xclbin_uuid, graph, am);
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

void
xclGraphClose(xclGraphHandle ghdl)
{
  try {
    api::xclGraphClose(ghdl);
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
}

int
xclGraphReset(xclGraphHandle ghdl)
{
  try {
    api::xclGraphReset(ghdl);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

uint64_t
xclGraphTimeStamp(xclGraphHandle ghdl)
{
  try {
    return api::xclGraphTimeStamp(ghdl);
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphRun(xclGraphHandle ghdl, int iterations)
{
  try {
    api::xclGraphRun(ghdl, iterations);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphWaitDone(xclGraphHandle ghdl, int timeout_ms)
{
  try {
    api::xclGraphWaitDone(ghdl, timeout_ms);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphWait(xclGraphHandle ghdl, uint64_t cycle)
{
  try {
    api::xclGraphWait(ghdl, cycle);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphSuspend(xclGraphHandle ghdl)
{
  try {
    api::xclGraphSuspend(ghdl);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphResume(xclGraphHandle ghdl)
{
  try {
    api::xclGraphResume(ghdl);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphEnd(xclGraphHandle ghdl, uint64_t cycle)
{
  try {
    api::xclGraphEnd(ghdl, cycle);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphUpdateRTP(xclGraphHandle ghdl, const char* port, const char* buffer, size_t size)
{
  try {
    api::xclGraphUpdateRTP(ghdl, port, buffer, size);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphReadRTP(xclGraphHandle ghdl, const char *port, char *buffer, size_t size)
{
  try {
    api::xclGraphReadRTP(ghdl, port, buffer, size);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclSyncBOAIE(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  try {
    api::xclSyncBOAIE(handle, bo, gmioName, dir, size, offset);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclResetAIEArray(xclDeviceHandle handle)
{
  try {
    api::xclResetAieArray(handle);
    return 0;
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

////////////////////////////////////////////////////////////////
// Exposed for Vitis aietools as extensions to xrt_aie.h
////////////////////////////////////////////////////////////////
/**
 * xclSyncBOAIENB() - Transfer data between DDR and Shim DMA channel
 *
 * @handle:          Handle to the device
 * @bohdl:           BO handle.
 * @gmioName:        GMIO port name
 * @dir:             GM to AIE or AIE to GM
 * @size:            Size of data to synchronize
 * @offset:          Offset within the BO
 *
 * Return:          0 on success, -1 on error.
 *
 * Synchronize the buffer contents between GMIO and AIE.
 * Note: Upon return, the synchronization is submitted or error out
 */
int
xclSyncBOAIENB(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  try {
    api::xclSyncBOAIENB(handle, bo, gmioName, dir, size, offset);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

/**
 * xclGMIOWait() - Wait a shim DMA channel to be idle for a given GMIO port
 *
 * @handle:          Handle to the device
 * @gmioName:        GMIO port name
 *
 * Return:          0 on success, -1 on error.
 */
int
xclGMIOWait(xclDeviceHandle handle, const char *gmioName)
{
  try {
    api::xclGMIOWait(handle, gmioName);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclStartProfiling(xclDeviceHandle handle, int option, const char* port1Name, const char* port2Name, uint32_t value)
{
  try {

#ifndef __AIESIM__
#ifndef __HWEM__
    xdp::aie::finish_flush_device(handle) ;
    xdp::aie::ctr::end_poll(handle);
#endif
#endif

    return api::xclStartProfiling(handle, option, port1Name, port2Name, value);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return errno = ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return errno = 0;
  }
}

uint64_t
xclReadProfiling(xclDeviceHandle handle, int phdl)
{
  try {

#ifndef __AIESIM__
#ifndef __HWEM__
    xdp::aie::finish_flush_device(handle) ;
    xdp::aie::ctr::end_poll(handle);
#endif
#endif

    return api::xclReadProfiling(handle, phdl);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return errno = ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return errno = 0;
  }
}

int
xclStopProfiling(xclDeviceHandle handle, int phdl)
{
  try {

#ifndef __AIESIM__
#ifndef __HWEM__
    xdp::aie::finish_flush_device(handle) ;
    xdp::aie::ctr::end_poll(handle);
#endif
#endif

    api::xclStopProfiling(handle, phdl);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return errno = ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return errno = 0;
  }
}
