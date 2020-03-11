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
#include "core/edge/user/shim.h"
#include "core/include/experimental/xrt_aie.h"
#include "core/common/error.h"

#include <iostream>
#include <chrono>
#include <cerrno>

extern "C"
{
#include <xaiengine.h>
}

namespace zynqaie {

graph_type::
graph_type(std::shared_ptr<xrt_core::device> dev, uuid_t, const std::string& graph_name)
  : device(std::move(dev)), name(graph_name)
{
    // TODO
    // this is not the right place for creating Aie instance. Should
    // we move this to loadXclbin when detect Aie Array?
    auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());
    aieArray = drv->getAieArray();
    if (!aieArray) {
      aieArray = new Aie();
      drv->setAieArray(aieArray);
    }

    for (auto& tile : xrt_core::edge::aie::get_tiles(device.get(), name))
      tiles.emplace_back(std::move(tile));

    // to be removed once TBD decided
    tiles.emplace_back(tile_type{2, 4, 2, 4, 0x17e4});

    /* TODO
     * get RTP port from xclbin. Will need some interfaces like
     * xrt_core::xclbin::get_rtpports(const axlf *top, char *graphName);
     * just hard code for now.
     */
    rtps.emplace_back(rtp_type{"mygraph.k1.in[0]",

                               1,
                               0,
                               0,
                               0x4000,

                               2,
                               0,
                               0,
                               0,

                               2,
                               0,
                               1,
                               0x2000,

                               false,
                               true,
                               false,
                               false,
                               true});

    state = graph_state::stop;
}

graph_type::
~graph_type()
{
    /* TODO move this to ZYNQShim destructor or use smart pointer */
    if (aieArray)
        delete aieArray;
}


void
graph_type::
reset()
{
    for (auto& tile : tiles) {
      auto pos = aieArray->getTilePos(tile.col, tile.row);
      XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_DISABLE, XAIE_ENABLE);
    }

    state = graph_state::reset;
}

uint64_t
graph_type::
get_timestamp()
{
    /* TODO just use the first tile to get the timestamp? */
    auto& tile = tiles.at(0);
    auto pos = aieArray->getTilePos(tile.col, tile.row);

    uint64_t timeStamp = XAieTile_CoreReadTimer(&(aieArray->tileArray.at(pos)));
    return timeStamp;
}

void
graph_type::
run()
{
    if (state != graph_state::stop && state != graph_state::reset)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is already running");

    if (state != graph_state::reset)
      // Reset the graph first
      reset();

    for (auto& tile : tiles) {
        auto pos = aieArray->getTilePos(tile.col, tile.row);
        XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_ENABLE, XAIE_DISABLE);
    }

    state = graph_state::running;
}

void
graph_type::
update_iter(int iterations)
{
    for (auto& tile : tiles) {
        auto pos = aieArray->getTilePos(tile.itr_mem_col, tile.itr_mem_row);
        uint32_t addr = tile.itr_mem_addr;
        XAieTile_DmWriteWord(&(aieArray->tileArray.at(pos)), addr, iterations);
    }
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
            auto pos = aieArray->getTilePos(tile.itr_mem_col, tile.itr_mem_row);
            done = XAieTile_CoreReadStatusDone(&(aieArray->tileArray.at(pos)));
            if (!done)
                break;
        }

        if (done)
          return;

        auto current = std::chrono::high_resolution_clock::now();
        auto dur = current - begin;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
        if (timeout_ms >= 0 && timeout_ms < ms)
          throw xrt_core::error(-ETIME, "Wait graph '" + name + "' timeout.");
    }

    state = graph_state::stop;
}

void
graph_type::
suspend()
{
    if (state != graph_state::running)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot suspend");

    for (auto& tile : tiles) {
        auto pos = aieArray->getTilePos(tile.col, tile.row);
        XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_DISABLE, XAIE_DISABLE);
    }

    state = graph_state::suspend;
}

void
graph_type::
resume()
{
    if (state != graph_state::suspend)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not suspended, cannot resume");

    for (auto& tile : tiles) {
        auto pos = aieArray->getTilePos(tile.col, tile.row);
        XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_ENABLE, XAIE_DISABLE);
    }

    state = graph_state::running;
}

void
graph_type::
stop(int timeout_ms)
{
    if (state != graph_state::running)
      throw xrt_core::error(-EINVAL, "Graph '" + name + "' is not running, cannot stop");

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
            auto pos = aieArray->getTilePos(tile.itr_mem_col, tile.itr_mem_row);
            done = XAieTile_CoreReadStatusDone(&(aieArray->tileArray.at(pos)));
            if (!done)
                break;
        }

        if (done)
            return;

        auto current = std::chrono::high_resolution_clock::now();
        auto dur = current - begin;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
        if (timeout_ms >= 0 && timeout_ms < ms) {
            std::cout << "Wait Graph " << name << " timeout. Force stop" << std::endl;
            for (auto& tile : tiles) {
                auto pos = aieArray->getTilePos(tile.itr_mem_col, tile.itr_mem_row);
                XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_DISABLE, XAIE_DISABLE);

               /* TODO any extra action needed to end a graph? */
            }

            break;
        }
    }

    state = graph_state::stop;
}

void
graph_type::
update_rtp(const char* port, const char* buffer, size_t size)
{
    auto rtp = std::find_if(rtps.begin(), rtps.end(),
            [port](rtp_type it) { return it.name.compare(port) == 0; });

    if (rtp != rtps.end())
      throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': RTP port '" + port + "' not found");

    if (rtp->is_plrtp)
      throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': not aie rtp port");
 
    /* We only support input RTP */
    if (!rtp->is_input)
      throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': only input RTP port supported");
 
    /*
     * Only support sync update
     * TODO support async update
     */
    if (rtp->is_async)
      throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': only synchronous update supported");
 
    /* If RTP port is connected, only support async update */
    if (rtp->is_connected)
      throw xrt_core::error(-EINVAL, "Can't update graph '" + name + "': connected sync update is not supported");
 
    auto selector_pos = aieArray->getTilePos(rtp->selector_col, rtp->selector_row);
    auto selector_tile = &(aieArray->tileArray.at(selector_pos));

    if (rtp->require_lock)
        XAieTile_LockAcquire(selector_tile, rtp->selector_lock_id, 0, 0xFFFFFFFF);

    uint32_t selector = XAieTile_DmReadWord(selector_tile, rtp->selector_addr);
    selector = 1 - selector;

    int update_pos;
    uint16_t lock_id;
    uint64_t start_addr;
    if (selector == 1) {
        /* update pong buffer */
        update_pos = aieArray->getTilePos(rtp->pong_col, rtp->pong_row);
        lock_id = rtp->pong_lock_id;
        start_addr = rtp->pong_addr;
    } else {
        /* update ping buffer */
        update_pos = aieArray->getTilePos(rtp->ping_col, rtp->ping_row);
        lock_id = rtp->ping_lock_id;
        start_addr = rtp->ping_addr;
    }

   XAieGbl_Tile *update_tile = &(aieArray->tileArray.at(update_pos));
   if (rtp->require_lock)
        XAieTile_LockAcquire(update_tile, lock_id, 0, 0xFFFFFFFF);

    size_t iterations = size / 4;
    size_t remain = size % 4;
    int i;
    for (i = 0; i < iterations; ++i) {
        XAieTile_DmWriteWord(update_tile, start_addr, ((const u32*)buffer)[i]);
        start_addr += 4;
    }
    if (remain) {
        uint32_t rdata = 0;
        memcpy(&rdata, &((const u32*)buffer)[i], remain);
        XAieTile_DmWriteWord(update_tile, start_addr, rdata);
    }

    /* update selector */
    XAieTile_DmWriteWord(selector_tile, rtp->selector_addr, selector);

    if (rtp->require_lock) {
        /* release lock */
        XAieTile_LockRelease(selector_tile, rtp->selector_lock_id, 1, 0XFFFFFFFF);
        XAieTile_LockRelease(update_tile, lock_id, 1, 0XFFFFFFFF);
    }
}


} // zynqaie

namespace {

using graph_type = zynqaie::graph_type;

// Active graphs per xrtGraphOpen/Close.  This is a mapping from
// xrtGraphHandle to the corresponding graph object. xrtGraphHandles
// is the address of the graph object.  This is shared ownership, as
// internals can use the graph object while applicaiton has closed the
// correspoding handle. The map content is deleted when user closes
// the handle, but underlying graph object may remain alive per
// reference count.
static std::map<xrtGraphHandle, std::shared_ptr<graph_type>> graphs;

static std::shared_ptr<graph_type>
get_graph(xrtGraphHandle ghdl)
{
  auto itr = graphs.find(ghdl);
  if (itr == graphs.end())
    throw std::runtime_error("Unknown graph handle");
  return (*itr).second;
}

}

namespace api {

using graph_type = zynqaie::graph_type;

xrtGraphHandle
xrtGraphOpen(xclDeviceHandle dhdl, uuid_t xclbin_uuid, const char* name)
{
  auto device = xrt_core::get_userpf_device(dhdl);
  auto graph = std::make_shared<graph_type>(device, xclbin_uuid, name);
  auto handle = graph.get();
  graphs.emplace(std::make_pair(handle,std::move(graph)));
  return handle;
}

void
xrtGraphClose(xrtGraphHandle ghdl)
{
  auto graph = get_graph(ghdl);
  graphs.erase(graph.get());
}

void
xrtGraphReset(xrtGraphHandle ghdl)
{
  auto graph = get_graph(ghdl);
  graph->reset();
}
  
uint64_t
xrtGraphTimeStamp(xrtGraphHandle ghdl)
{
  auto graph = get_graph(ghdl);
  return graph->get_timestamp();
}

void
xrtGraphRun(xrtGraphHandle ghdl)
{
  auto graph = get_graph(ghdl);
  graph->run();
}

void
xrtGraphUpdateIter(xrtGraphHandle ghdl, int iterations)
{
  auto graph = get_graph(ghdl);
  graph->update_iter(iterations);
}

void
xrtGraphWaitDone(xrtGraphHandle ghdl, int timeout_ms)
{
  auto graph = get_graph(ghdl);
  graph->wait_done(timeout_ms);
}

void
xrtGraphSuspend(xrtGraphHandle ghdl)
{
  auto graph = get_graph(ghdl);
  graph->suspend();
}

void
xrtGraphResume(xrtGraphHandle ghdl)
{
  auto graph = get_graph(ghdl);
  graph->resume();
}

void
xrtGraphStop(xrtGraphHandle ghdl, int timeout_ms)
{
  auto graph = get_graph(ghdl);
  graph->stop(timeout_ms);
}

void
xrtGraphUpdateRTP(xrtGraphHandle ghdl, const char* port, const char* buffer, size_t size)
{
  auto graph = get_graph(ghdl);
  graph->update_rtp(port, buffer, size);
}

} // api
  

////////////////////////////////////////////////////////////////
// xrt_aie API implementations (xrt_aie.h)
////////////////////////////////////////////////////////////////
xrtGraphHandle
xrtGraphOpen(xclDeviceHandle handle, uuid_t xclbin_uuid, const char* graph)
{
  try {
    return api::xrtGraphOpen(handle, xclbin_uuid, graph);
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
    
}

void
xrtGraphClose(xrtGraphHandle ghdl)
{
  try {
    api::xrtGraphClose(ghdl);
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
}

int
xrtGraphReset(xrtGraphHandle ghdl)
{
  try {
    api::xrtGraphReset(ghdl);
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
xrtGraphTimeStamp(xrtGraphHandle ghdl)
{
  try {
    return api::xrtGraphTimeStamp(ghdl);
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xrtGraphRun(xrtGraphHandle ghdl)
{
  try {
    api::xrtGraphRun(ghdl);
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
xrtGraphUpdateIter(xrtGraphHandle ghdl, int iterations)
{
  try {
    api::xrtGraphUpdateIter(ghdl, iterations);
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
xrtGraphWaitDone(xrtGraphHandle ghdl, int timeout_ms)
{
  try {
    api::xrtGraphWaitDone(ghdl, timeout_ms);
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
xrtGraphSuspend(xrtGraphHandle ghdl)
{
  try {
    api::xrtGraphSuspend(ghdl);
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
xrtGraphResume(xrtGraphHandle ghdl)
{
  try {
    api::xrtGraphResume(ghdl);
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
xrtGraphStop(xrtGraphHandle ghdl, int timeout_ms)
{
  try {
    api::xrtGraphStop(ghdl, timeout_ms);
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
xrtGraphUpdateRTP(xrtGraphHandle ghdl, const char* port, const char* buffer, size_t size)
{
  try {
    api::xrtGraphUpdateRTP(ghdl, port, buffer, size);
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
