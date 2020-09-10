/**
 * Copyright (C) 2020 Xilinx, Inc
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
graph_type(std::shared_ptr<xrt_core::device> dev, const uuid_t, const std::string& graph_name)
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

    /* Initialize graph tile metadata */
    for (auto& tile : xrt_core::edge::aie::get_tiles(device.get(), name)) {
      /*
       * Since row 0 is shim row, according to Vitis aietools, row data in
       * xclbin is off-by-one. To talk to AIE driver, we need to add
       * shim row back.
       */
      tile.row += 1;
      tile.itr_mem_row += 1;
      tiles.emplace_back(std::move(tile));
    }

    /* Initialize graph rtp metadata */
    for (auto &rtp : xrt_core::edge::aie::get_rtp(device.get())) {
      rtp.selector_row += 1;
      rtp.ping_row += 1;
      rtp.pong_row += 1;
      rtps.emplace_back(std::move(rtp));
    }

    state = graph_state::reset;
    startTime = 0;
}

graph_type::
~graph_type()
{
}

void
graph_type::
reset()
{
    for (auto& tile : tiles) {
      XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
      XAie_CoreDisable(aieArray->getDevInst(), coreTile);
    }

    state = graph_state::reset;
}

uint64_t
graph_type::
get_timestamp()
{
    /* TODO just use the first tile to get the timestamp? */
    auto& tile = tiles.at(0);
    XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);

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
    if (state != graph_state::stop && state != graph_state::reset)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is already running or has ended");

    /* Record a snapshot of graph start time */
    if (!tiles.empty()) {
        auto& tile = tiles.at(0);
        XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
        XAie_ReadTimer(aieArray->getDevInst(), coreTile, XAIE_CORE_MOD, &startTime);
    }

    for (auto& tile : tiles) {
        XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
        XAie_CoreEnable(aieArray->getDevInst(), coreTile);
    }

    state = graph_state::running;
}

void
graph_type::
run(int iterations)
{
    if (state != graph_state::stop && state != graph_state::reset)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is already running or has ended");

    for (auto& tile : tiles) {
        XAie_LocType memTile = XAie_TileLoc(tile.itr_mem_col, tile.itr_mem_row);
        XAie_DataMemWrWord(aieArray->getDevInst(), memTile, tile.itr_mem_addr, iterations);
    }

    /* Record a snapshot of graph start time */
    if (!tiles.empty()) {
        auto& tile = tiles.at(0);
        XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
        XAie_ReadTimer(aieArray->getDevInst(), coreTile, XAIE_CORE_MOD, &startTime);
    }

    for (auto& tile : tiles) {
        XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
        XAie_CoreEnable(aieArray->getDevInst(), coreTile);
    }

    state = graph_state::running;
}

void
graph_type::
wait_done(int timeout_ms)
{
    if (state != graph_state::running)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot wait");

    auto begin = std::chrono::high_resolution_clock::now();

    /*
     * We are using busy waiting here. Until every tile in the graph
     * is done, we keep polling each tile.
     *
     * TODO Will register AIE event for tile done.
     */
    while (1) {
        uint8_t done;
        for (auto& tile : tiles) {
            /* Skip multi-rate core */
            if (tile.is_trigger) {
                done = 1;
                continue;
            }

            XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
            XAie_CoreReadDoneBit(aieArray->getDevInst(), coreTile, &done);
            if (!done)
                break;
        }

        if (done) {
            state = graph_state::stop;
            for (auto& tile : tiles) {
                if (tile.is_trigger)
                    continue;

                XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
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
    if (state != graph_state::running)
        throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot wait");

    for (auto& tile : tiles) {
        if (tile.is_trigger)
            continue;

        XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
        while (1) {
            if (XAie_CoreWaitForDone(aieArray->getDevInst(), coreTile, 0) == XAIE_OK)
                break;
        }

        XAie_CoreDisable(aieArray->getDevInst(), coreTile);
    }

    state = graph_state::stop;
}

void
graph_type::
wait(uint64_t cycle)
{
    if (state != graph_state::running)
        throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot wait");

    // Adjust the cycle-timeout value
    if (!tiles.empty()) {
        auto& tile = tiles.at(0);

        uint64_t elapsed_time;
        XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
        XAie_ReadTimer(aieArray->getDevInst(), coreTile, XAIE_CORE_MOD, &elapsed_time);
        elapsed_time -= startTime;

        if (cycle > elapsed_time)
            XAie_WaitCycles(aieArray->getDevInst(), coreTile, XAIE_CORE_MOD, (cycle - elapsed_time));
    }

    for (auto& tile : tiles)
    {
        XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
        XAie_CoreDisable(aieArray->getDevInst(), coreTile);
    }

    state = graph_state::suspend;
}

void
graph_type::
suspend()
{
    if (state != graph_state::running)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot suspend");

    for (auto& tile : tiles) {
        XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
        XAie_CoreDisable(aieArray->getDevInst(), coreTile);
    }

    state = graph_state::suspend;
}

void
graph_type::
resume()
{
    if (state != graph_state::suspend)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not suspended (wait(cycle)), cannot resume");

    if (!tiles.empty()) {
        auto& tile = tiles.at(0);
        XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
        XAie_ReadTimer(aieArray->getDevInst(), coreTile, XAIE_CORE_MOD, &startTime);
    }

    for (auto& tile : tiles) {
        XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);

        /*
         * We only resume the core that is not in done status.
         * XAIE_ENABLE will clear Core_Done status bit.
         */
        uint8_t done;
        XAie_CoreReadDoneBit(aieArray->getDevInst(), coreTile, &done);
        if (!done)
            XAie_CoreEnable(aieArray->getDevInst(), coreTile);
    }

    state = graph_state::running;
}

void
graph_type::
end()
{
    if (state != graph_state::running && state != graph_state::stop)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running or stop, cannot end");

    /* Wait for graph done first. The state will be set to stop */
    if (state == graph_state::running)
        wait();

    for (auto& tile : tiles) {
        if (tile.is_trigger)
            continue;

        /* Set sync buf to trigger the end procedure */
        XAie_LocType memTile = XAie_TileLoc(tile.itr_mem_col, tile.itr_mem_row);
        XAie_DataMemWrWord(aieArray->getDevInst(), memTile, tile.itr_mem_addr - 4, (u32)1);

        XAie_LocType coreTile = XAie_TileLoc(tile.col, tile.row);
        XAie_CoreEnable(aieArray->getDevInst(), coreTile);

        while (1) {
            if (XAie_CoreWaitForDone(aieArray->getDevInst(), coreTile, 0) == XAIE_OK)
                break;
        }

        XAie_CoreDisable(aieArray->getDevInst(), coreTile);
    }

    state = graph_state::end;
}

void
graph_type::
end(uint64_t cycle)
{
    if (state != graph_state::running && state != graph_state::suspend)
        throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running or suspended, cannot end(cycle_timeout)");

    /* Wait(cycle) will suspend graph. */
    if (state == graph_state::running)
        wait(cycle);

    for (auto& tile : tiles) {
        /* Set sync buf to trigger the end procedure */
        XAie_LocType memTile = XAie_TileLoc(tile.itr_mem_col, tile.itr_mem_row);
        XAie_DataMemWrWord(aieArray->getDevInst(), memTile, tile.itr_mem_addr - 4, (u32)1);
    }

    state = graph_state::end;
}

#define LOCK_TIMEOUT 0x7FFFFFFF
#define ACQ_WRITE    0
#define ACQ_READ     1
#define REL_READ     1
#define REL_WRITE    0

void
graph_type::
update_rtp(const std::string& port, const char* buffer, size_t size)
{
    auto rtp = std::find_if(rtps.begin(), rtps.end(),
            [port](rtp_type it) { return it.name.compare(port) == 0; });

    if (rtp == rtps.end())
      throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': RTP port '" + port + "' not found");

    if (rtp->is_plrtp)
      throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': RTP port '" + port + "' is not AIE RTP");

    if (!rtp->is_input)
      throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': RTP port '" + port + "' is not input");

    /* If RTP port is connected, only support async update */
    if (rtp->is_connected) {
        if (rtp->is_async && state == graph_state::running)
            throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': updating connected async RTP '" + port + "' is not supported while graph is running");
        if (!rtp->is_async)
            throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': updating connected sync RTP '" + port + "' is not supported");
    }

    /* Don't acquire selector lock for async RTP while graph is not running */
    bool need_lock = rtp->require_lock && (
        rtp->is_async && state == graph_state::running ||
        !rtp->is_async
	);

    XAie_LocType selector_tile = XAie_TileLoc(rtp->selector_col, rtp->selector_row);
    XAie_Lock selector_lock = XAie_LockInit(rtp->selector_lock_id, (rtp->is_async ? 0xFF : ACQ_WRITE));

    if (need_lock) {
        AieRC rc = XAie_LockAcquire(aieArray->getDevInst(), selector_tile, selector_lock, LOCK_TIMEOUT);
        if (rc != XAIE_OK)
            throw xrt_core::error(-EIO, "Can't update graph '" + name + "': acquire lock for RTP '" + port + "' failed or timeout");
    }

    uint32_t selector;
    XAie_DataMemRdWord(aieArray->getDevInst(), selector_tile, rtp->selector_addr, &selector);

    selector = 1 - selector;

    XAie_LocType update_tile;
    uint16_t lock_id;
    uint64_t start_addr;
    if (selector == 1) {
        /* update pong buffer */
        update_tile = XAie_TileLoc(rtp->pong_col, rtp->pong_row);
        lock_id = rtp->pong_lock_id;
        start_addr = rtp->pong_addr;
    } else {
        /* update ping buffer */
        update_tile = XAie_TileLoc(rtp->ping_col, rtp->ping_row);
        lock_id = rtp->ping_lock_id;
        start_addr = rtp->ping_addr;
    }

    XAie_Lock update_lock = XAie_LockInit(lock_id, (rtp->is_async ? 0xFF : ACQ_WRITE));
    if (need_lock) {
        AieRC rc = XAie_LockAcquire(aieArray->getDevInst(), update_tile, update_lock, LOCK_TIMEOUT);
        if (rc != XAIE_OK)
            throw xrt_core::error(-EIO, "Can't update graph '" + name + "': acquire lock for RTP '" + port + "' failed or timeout");
    }

    size_t iterations = size / 4;
    size_t remain = size % 4;
    int i;

    for (i = 0; i < iterations; ++i) {
        XAie_DataMemWrWord(aieArray->getDevInst(), update_tile, start_addr, ((const uint32_t *)buffer)[i]);
        start_addr += 4;
    }
    if (remain) {
        uint32_t rdata = 0;
        memcpy(&rdata, &((const uint32_t *)buffer)[i], remain);
        XAie_DataMemWrWord(aieArray->getDevInst(), update_tile, start_addr, rdata);
    }

    /* update selector */
    XAie_DataMemWrWord(aieArray->getDevInst(), selector_tile, rtp->selector_addr, selector);

    if (rtp->require_lock) {
        /* release lock, need to release lock even graph is not running */
        selector_lock = XAie_LockInit(rtp->selector_lock_id, REL_READ);
        uint8_t rc = XAie_LockRelease(aieArray->getDevInst(), selector_tile, selector_lock, LOCK_TIMEOUT);
        if (rc != XAIE_OK)
            throw xrt_core::error(-EIO, "Can't update graph '" + name + "': release lock for RTP '" + port + "' failed or timeout");

        update_lock = XAie_LockInit(lock_id, REL_READ);
        rc = XAie_LockRelease(aieArray->getDevInst(), update_tile, update_lock, LOCK_TIMEOUT);
        if (rc != XAIE_OK)
            throw xrt_core::error(-EIO, "Can't update graph '" + name + "': release lock for RTP '" + port + "' failed or timeout");
    }
}

void
graph_type::
read_rtp(const std::string& port, char* buffer, size_t size)
{
    auto rtp = std::find_if(rtps.begin(), rtps.end(),
            [port](rtp_type it) { return it.name.compare(port) == 0; });

    if (rtp == rtps.end())
      throw xrt_core::error(-EINVAL, "Can't read graph '" + name + "': RTP port '" + port + "' not found");

    if (rtp->is_plrtp)
      throw xrt_core::error(-EINVAL, "Can't read graph '" + name + "': RTP port '" + port + "' is not AIE RTP");

    if (rtp->is_input)
      throw xrt_core::error(-EINVAL, "Can't read graph '" + name + "': RTP port '" + port + "' is input");

    /* If RTP port is connected, only support async update */
    if (rtp->is_connected)
      throw xrt_core::error(-EINVAL, "Can't read graph '" + name + "': reading connected RTP port '" + port + "' is not supported");

    /* Don't acquire selector lock for async RTP while graph is not running */
    bool need_lock = rtp->require_lock && (
        rtp->is_async && state == graph_state::running ||
        !rtp->is_async
        );

    XAie_LocType selector_tile = XAie_TileLoc(rtp->selector_col, rtp->selector_row);
    XAie_Lock selector_lock = XAie_LockInit(rtp->selector_lock_id, ACQ_READ);

    if (need_lock) {
        AieRC rc = XAie_LockAcquire(aieArray->getDevInst(), selector_tile, selector_lock, LOCK_TIMEOUT);
        if (rc != XAIE_OK)
            throw xrt_core::error(-EIO, "Can't read graph '" + name + "': acquire lock for RTP '" + port + "' failed or timeout");
    }

    uint32_t selector;
    XAie_DataMemRdWord(aieArray->getDevInst(), selector_tile, rtp->selector_addr, &selector);

    XAie_LocType update_tile;
    uint16_t lock_id;
    uint64_t start_addr;
    if (selector == 1) {
        /* update pong buffer */
        update_tile = XAie_TileLoc(rtp->pong_col, rtp->pong_row);
        lock_id = rtp->pong_lock_id;
        start_addr = rtp->pong_addr;
    } else {
        /* update ping buffer */
        update_tile = XAie_TileLoc(rtp->ping_col, rtp->ping_row);
        lock_id = rtp->ping_lock_id;
        start_addr = rtp->ping_addr;
    }

    XAie_Lock update_lock = XAie_LockInit(lock_id, ACQ_READ);
    if (need_lock) {
        AieRC rc = XAie_LockAcquire(aieArray->getDevInst(), update_tile, update_lock, LOCK_TIMEOUT);
        if (rc != XAIE_OK)
            throw xrt_core::error(-EIO, "Can't read graph '" + name + "': acquire lock for RTP '" + port + "' failed or timeout");

	/* sync RTP release lock for write, async RTP relase lock for read */
        selector_lock = XAie_LockInit(rtp->selector_lock_id, (rtp->is_async ? REL_READ : REL_WRITE));
        rc = XAie_LockRelease(aieArray->getDevInst(), selector_tile, selector_lock, LOCK_TIMEOUT);
        if (rc != XAIE_OK)
            throw xrt_core::error(-EIO, "Can't read graph '" + name + "': release lock for RTP '" + port + "' failed or timeout");
    }

    size_t iterations = size / 4;
    size_t remain = size % 4;
    int i;

    for (i = 0; i < iterations; ++i) {
        XAie_DataMemRdWord(aieArray->getDevInst(), update_tile, start_addr, (u32*)&(((u32*)buffer)[i]));
        start_addr += 4;
    }
    if (remain) {
        uint32_t rdata;
        XAie_DataMemRdWord(aieArray->getDevInst(), update_tile, start_addr, ((u32*)&rdata));
        memcpy(&((uint32_t *)buffer)[i], &rdata, remain);
    }

    if (need_lock) {
        /* release lock */
        update_lock = XAie_LockInit(lock_id, (rtp->is_async ? REL_READ : REL_WRITE));
        AieRC rc = XAie_LockRelease(aieArray->getDevInst(), update_tile, update_lock, LOCK_TIMEOUT);
        if (rc != XAIE_OK)
            throw xrt_core::error(-EIO, "Can't read graph '" + name + "': release lock for RTP '" + port + "' failed or timeout");
    }
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

xclGraphHandle
xclGraphOpen(xclDeviceHandle dhdl, const uuid_t xclbin_uuid, const char* name)
{
  auto device = xrt_core::get_userpf_device(dhdl);
  auto graph = std::make_shared<graph_type>(device, xclbin_uuid, name);
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
xclSyncBOAIE(xclDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
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

  auto bosize = xrtBOSize(bohdl);

  if (offset + size > bosize)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: exceed BO boundary.");

  aieArray->sync_bo(bohdl, gmioName, dir, size, offset);
}

void
xclSyncBOAIENB(xclDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
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

  auto bosize = xrtBOSize(bohdl);

  if (offset + size > bosize)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: exceed BO boundary.");

  aieArray->sync_bo_nb(bohdl, gmioName, dir, size, offset);
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
  /* Do a handle check */
  xrt_core::get_userpf_device(handle);

}

} // api


////////////////////////////////////////////////////////////////
// Shim level Graph API implementations (xrt_graph.h)
////////////////////////////////////////////////////////////////
xclGraphHandle
xclGraphOpen(xclDeviceHandle handle, const uuid_t xclbin_uuid, const char* graph)
{
  try {
    return api::xclGraphOpen(handle, xclbin_uuid, graph);
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
xclSyncBOAIE(xclDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  try {
    api::xclSyncBOAIE(handle, bohdl, gmioName, dir, size, offset);
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
xclSyncBOAIENB(xclDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  try {
    api::xclSyncBOAIENB(handle, bohdl, gmioName, dir, size, offset);
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
