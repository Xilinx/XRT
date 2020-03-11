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

#include <iostream>
#include <chrono>
#include <cerrno>

extern "C"
{
#include <xaiengine.h>
}

namespace zynqaie {

Graph::Graph(xclDeviceHandle handle, const std::string& graphName)
  : name(graphName)
{
    auto drv = ZYNQ::shim::handleCheck(handle);
    devHandle = handle;

    /*
     * TODO
     * this is not the right place for creating Aie instance. Should
     * we move this to loadXclbin when detect Aie Array?
     */
    aieArray = drv->getAieArray();
    if (!aieArray) {
        aieArray = new Aie();
        drv->setAieArray(aieArray);
    }

    auto device = xrt_core::get_userpf_device(handle);
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

    state = GRAPH_STATE_STOP;
}

Graph::~Graph()
{
    /* TODO move this to ZYNQShim destructor or use smart pointer */
    if (aieArray)
        delete aieArray;
}

Graph *Graph::graphHandleCheck(void *gHandle)
{
    if (!gHandle)
        return 0;

    return (Graph *) gHandle;
}

int Graph::xrtGraphReset()
{
    for (auto& tile : tiles) {
      auto pos = aieArray->getTilePos(tile.col, tile.row);
      XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_DISABLE, XAIE_ENABLE);
    }

    state = GRAPH_STATE_RESET;

  return 0;
}

uint64_t Graph::xrtGraphTimeStamp()
{
    /* TODO just use the first tile to get the timestamp? */
    auto& tile = tiles.at(0);
    auto pos = aieArray->getTilePos(tile.col, tile.row);

    uint64_t timeStamp = XAieTile_CoreReadTimer(&(aieArray->tileArray.at(pos)));
    return timeStamp;
}

int Graph::xrtGraphRun()
{
    if (state != GRAPH_STATE_STOP && state != GRAPH_STATE_RESET) {
        std::cout << "Error: xrtGraphRun graph fail, already running" << std::endl;
	return -EINVAL;
    }

    if (state != GRAPH_STATE_RESET) {
        /* Reset the graph first */
        xrtGraphReset();
    }

    for (auto& tile : tiles) {
        auto pos = aieArray->getTilePos(tile.col, tile.row);
        XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_ENABLE, XAIE_DISABLE);
    }

    state = GRAPH_STATE_RUNNING;
 
    return 0;
}

int Graph::xrtGraphUpdateIter(int iterations)
{
    for (auto& tile : tiles) {
        auto pos = aieArray->getTilePos(tile.itr_mem_col, tile.itr_mem_row);
        uint32_t addr = tile.itr_mem_addr;
        XAieTile_DmWriteWord(&(aieArray->tileArray.at(pos)), addr, iterations);
    }

    return 0;
}

int Graph::xrtGraphWaitDone(int timeoutMilliSec)
{
    if (state != GRAPH_STATE_RUNNING)
        return -EINVAL;

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
            return 0;

        auto current = std::chrono::high_resolution_clock::now();
        auto dur = current - begin;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
        if (timeoutMilliSec >= 0 && timeoutMilliSec < ms) {
            std::cout << "Wait Graph " << name << " timeout." << std::endl;
            return -ETIME;
        }
    }

    state = GRAPH_STATE_STOP;

    return 0;
}

int Graph::xrtGraphSuspend()
{
    if (state != GRAPH_STATE_RUNNING) {
        std::cout << "Error: xrtGraphDisable graph fail, not running" << std::endl;
	return -EINVAL;
    }

    for (auto& tile : tiles) {
        auto pos = aieArray->getTilePos(tile.col, tile.row);
        XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_DISABLE, XAIE_DISABLE);
    }

    state = GRAPH_STATE_SUSPEND;

    return 0;
}

int Graph::xrtGraphResume()
{
    if (state != GRAPH_STATE_SUSPEND) {
        std::cout << "Error: xrtGraphEnable graph fail, not suspending" << std::endl;
        return -EINVAL;
    }

    for (auto& tile : tiles) {
        auto pos = aieArray->getTilePos(tile.col, tile.row);
        XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_ENABLE, XAIE_DISABLE);
    }

    state = GRAPH_STATE_RUNNING;

    return 0;
}

int Graph::xrtGraphStop(int timeoutMilliSec)
{
    if (state != GRAPH_STATE_RUNNING)
        return -EINVAL;

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
            return 0;

        auto current = std::chrono::high_resolution_clock::now();
        auto dur = current - begin;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
        if (timeoutMilliSec >= 0 && timeoutMilliSec < ms) {
            std::cout << "Wait Graph " << name << " timeout. Force stop" << std::endl;
            for (auto& tile : tiles) {
                auto pos = aieArray->getTilePos(tile.itr_mem_col, tile.itr_mem_row);
                XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_DISABLE, XAIE_DISABLE);

               /* TODO any extra action needed to end a graph? */
            }

            break;
        }
    }

    state = GRAPH_STATE_STOP;

    return 0;
}

int Graph::xrtGraphRTPUpdate(const char *hierPathPort, const char *buffer, size_t size)
{
    auto rtp = std::find_if(rtps.begin(), rtps.end(),
            [&](rtp_type it) { return it.name.compare(hierPathPort) == 0; });

    if (rtp != rtps.end()) {
        std::cout << "Can't update Graph " << name << ": RTP port " << hierPathPort << " not found." << std::endl;
        return -EINVAL;
    }

    if (rtp->is_plrtp) {
        std::cout << "Can't update Graph " << name << ": not aie rtp port." << std::endl;
        return -EINVAL;
    }
 
    /* We only support input RTP */
    if (!rtp->is_input) {
        std::cout << "Can't update Graph " << name << ": only support input RTP port." << std::endl;
        return -EINVAL;
    }
 
    /*
     * Only support sync update
     * TODO support async update
     */
    if (rtp->is_async) {
        std::cout << "Can't update Graph " << name << ": only support sync update." << std::endl;
        return -EINVAL;
    }
 
    /* If RTP port is connected, only support async update */
    if (rtp->is_connected) {
        std::cout << "Can't update Graph " << name << ": connected sync update is not supported." << std::endl;
        return -EINVAL;
    }
 
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

    return 0;
}


}

xrtGraphHandle xrtGraphOpen(xclDeviceHandle handle, uuid_t xclbinUUID, const    char *graphName)
{
    auto drv = ZYNQ::shim::handleCheck(handle);
    if (!drv)
        return NULL;

    zynqaie::Graph *gHandle = new zynqaie::Graph(handle, graphName);
    if (!zynqaie::Graph::graphHandleCheck(gHandle)) {
        delete gHandle;
        gHandle = 0;
    }

    return (xrtGraphHandle) gHandle;
}

void xrtGraphClose(xrtGraphHandle gh)
{
    if (zynqaie::Graph::graphHandleCheck(gh))
        delete ((zynqaie::Graph *) gh);
}

int xrtGraphReset(xrtGraphHandle gh)
{
    zynqaie::Graph *graph = zynqaie::Graph::graphHandleCheck(gh);
    if (!graph)
        return -EINVAL;

    return graph->xrtGraphReset();
}

/**
 * Get a timestamp
 */
uint64_t xrtGraphTimeStamp(xrtGraphHandle gh)
{
    zynqaie::Graph *graph = zynqaie::Graph::graphHandleCheck(gh);
    if (!graph)
        return -EINVAL;

    return graph->xrtGraphTimeStamp();
}

/**
 * Enable tiles and disable tile reset
 */
int xrtGraphRun(xrtGraphHandle gh)
{
    zynqaie::Graph *graph = zynqaie::Graph::graphHandleCheck(gh);
    if (!graph)
        return -EINVAL;

    return graph->xrtGraphRun();
}

/**
 * Update iter variable locations for all the tiles
 */
int xrtGraphUpdateIter(xrtGraphHandle gh, int iterations)
{
    zynqaie::Graph *graph = zynqaie::Graph::graphHandleCheck(gh);
    if (!graph)
        return -EINVAL;

    return graph->xrtGraphUpdateIter(iterations);
}

int xrtGraphWaitDone(xrtGraphHandle gh, int timeoutMilliSec)
{
    zynqaie::Graph *graph = zynqaie::Graph::graphHandleCheck(gh);
    if (!graph)
        return -EINVAL;

    return graph->xrtGraphWaitDone(timeoutMilliSec);
}

int xrtGraphSuspend(xrtGraphHandle gh)
{
    zynqaie::Graph *graph = zynqaie::Graph::graphHandleCheck(gh);
    if (!graph)
        return -EINVAL;

    return graph->xrtGraphSuspend();
}

int xrtGraphResume(xrtGraphHandle gh)
{
    zynqaie::Graph *graph = zynqaie::Graph::graphHandleCheck(gh);
    if (!graph)
        return -EINVAL;

    return graph->xrtGraphResume();
}

int xrtGraphStop(xrtGraphHandle gh, int timeoutMilliSec)
{
    zynqaie::Graph *graph = zynqaie::Graph::graphHandleCheck(gh);
    if (!graph)
        return -EINVAL;

    return graph->xrtGraphStop(timeoutMilliSec);
}

int xrtGraphRTPUpdate(xrtGraphHandle gh, const char *hierPathPort, const char *buffer, size_t size)
{
    zynqaie::Graph *graph = zynqaie::Graph::graphHandleCheck(gh);
    if (!graph)
        return -EINVAL;

    return graph->xrtGraphRTPUpdate(hierPathPort, buffer, size);
}
