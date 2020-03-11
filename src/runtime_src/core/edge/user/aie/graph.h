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

#ifndef _ZYNQ_GRAPH_H
#define _ZYNQ_GRAPH_H

#include "aie.h"
#include "xrt.h"
#include "core/edge/common/aie_parser.h"
#include <string>
#include <vector>

typedef xclDeviceHandle xrtDeviceHandle;

namespace zynqaie {

class Graph
{
public:
    using tile_type = xrt_core::edge::aie::tile;
    using rtp_type = xrt_core::edge::aie::rtp;

    ~Graph();
    Graph(xrtDeviceHandle handle, const std::string& graphName);

    static Graph *graphHandleCheck(void *gHandle);

    int xrtGraphReset();
    uint64_t xrtGraphTimeStamp();
    int xrtGraphUpdateIter(int iterations);
    int xrtGraphRun();
    int xrtGraphWaitDone(int timeoutMilliSec);
    int xrtGraphSuspend();
    int xrtGraphResume();
    int xrtGraphStop(int timeoutMilliSec);
    int xrtGraphRTPUpdate(const char *hierPathPort, const char *buffer, size_t size);

private:

    enum graphState {
        GRAPH_STATE_STOP = 0,
        GRAPH_STATE_RESET = 1,
        GRAPH_STATE_RUNNING = 2,
        GRAPH_STATE_SUSPEND = 3,
    };

    graphState state;

    std::string name;

    /**
     * This is the XRT device handle that the graph belongs to.
     * To operate on a graph, XRT device has to be opened first.'
     */
    xrtDeviceHandle devHandle;

    /**
     * This is the pointer to the AIE array where the AIE part of
     * the graph resides. The Aie is an obect that holds the whole
     * AIE resources, configurations etc.
     * TODO it should be initialized when we load XCLBIN?
     */
    Aie *aieArray;

    /**
     * This is the collections of tiles that this graph uses.
     * TODO A tile is represented by a pair of number <col, row>?
     * It represents the tile position in the AIE array.
     */
    std::vector<tile_type> tiles;

    std::vector<rtp_type> rtps;
};

}

#endif
