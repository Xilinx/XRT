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

extern "C"
{
#include <xaiengine.h>
}
#include "xrt_aie.h"
#include "shim.h"
#include "tile.h"
#include "graph.h"

#include <iostream>
#include <errno.h>
#include <chrono>

namespace zynqaie {

Graph::Graph(xclDeviceHandle handle, const char *graphName)
{
    ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
    devHandle = handle;

    name = graphName;

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

    /* TODO
     * get tiles from xclbin. Will need some interfaces like
     * xrt_core::xclbin::get_tiles(const axlf *top, char *graphName);
     * just hard code for now.
     */
    tiles.push_back(new Tile(2, 4, 2, 4, 0x17e4));

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
    for (auto iter = tiles.begin(); iter != tiles.end(); ++iter) {
        auto col = (*iter)->graphCol;
        auto row = (*iter)->graphRow;
        auto pos = aieArray->getTilePos(col, row);

        XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_DISABLE, XAIE_ENABLE);
    }

    state = GRAPH_STATE_RESET;

    return 0;
}

uint64_t Graph::xrtGraphTimeStamp()
{
    /* TODO just use the first tile to get the timestamp? */
    auto col = (*tiles.begin())->graphCol;
    auto row = (*tiles.begin())->graphRow;

    auto pos = aieArray->getTilePos(col, row);

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

    for (auto iter = tiles.begin(); iter != tiles.end(); ++iter) {
        auto col = (*iter)->graphCol;
        auto row = (*iter)->graphRow;
        auto pos = aieArray->getTilePos(col, row);
        XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_ENABLE, XAIE_DISABLE);
    }

    state = GRAPH_STATE_RUNNING;
 
    return 0;
}

int Graph::xrtGraphUpdateIter(int iterations)
{
    for (auto iter = tiles.begin(); iter != tiles.end(); ++iter) {
        auto col = (*iter)->iterCol;
        auto row = (*iter)->iterRow;
        auto pos = aieArray->getTilePos(col, row);
        uint32_t addr = (*iter)->iterMemAddr;
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
        for (auto iter = tiles.begin(); iter != tiles.end(); ++iter) {
            auto col = (*iter)->iterCol;
            auto row = (*iter)->iterRow;
            auto pos = aieArray->getTilePos(col, row);
            uint32_t addr = (*iter)->iterMemAddr;
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

    for (auto iter = tiles.begin(); iter != tiles.end(); ++iter) {
        auto col = (*iter)->graphCol;
        auto row = (*iter)->graphRow;
        auto pos = aieArray->getTilePos(col, row);
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

    for (auto iter = tiles.begin(); iter != tiles.end(); ++iter) {
        auto col = (*iter)->graphCol;
        auto row = (*iter)->graphRow;
        auto pos = aieArray->getTilePos(col, row);
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
        for (auto iter = tiles.begin(); iter != tiles.end(); ++iter) {
            auto col = (*iter)->iterCol;
            auto row = (*iter)->iterRow;
            auto pos = aieArray->getTilePos(col, row);
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
            for (auto iter = tiles.begin(); iter != tiles.end(); ++iter) {
                auto col = (*iter)->iterCol;
                auto row = (*iter)->iterRow;
                auto pos = aieArray->getTilePos(col, row);
                XAieTile_CoreControl(&(aieArray->tileArray.at(pos)), XAIE_DISABLE, XAIE_DISABLE);

               /* TODO any extra action needed to end a graph? */
            }

            break;
        }
    }

    state = GRAPH_STATE_STOP;

    return 0;
}


}

xrtGraphHandle xrtGraphOpen(xclDeviceHandle handle, uuid_t xclbinUUID, const    char *graphName)
{
    ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
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
