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
#include "core/common/device.h"
#include <string>
#include <vector>

typedef xclDeviceHandle xrtDeviceHandle;

namespace zynqaie {

class graph_type
{
public:
    using tile_type = xrt_core::edge::aie::tile_type;
    using rtp_type = xrt_core::edge::aie::rtp_type;
    using gmio_type = xrt_core::edge::aie::gmio_type;

    graph_type(std::shared_ptr<xrt_core::device> device, const uuid_t xclbin_uuid, const std::string& name);
    ~graph_type();

    void
    reset();

    uint64_t
    get_timestamp();

    void
    run();

    void
    run(uint32_t iterations);

    void
    wait_done(int timeout_ms);

    void
    wait();

    void
    wait(uint64_t cycle);

    void
    suspend();

    void
    resume();

    void
    end();

    void
    end(uint64_t cycle);

    void
    update_rtp(const char* path, const char* buffer, size_t size);

    void
    read_rtp(const char* path, char* buffer, size_t size);

    void
    sync_bo(unsigned bo, const char *dmaID, enum xclBOSyncDirection dir, size_t size, size_t offset);

    static void
    event_cb(struct XAieGbl *aie_inst, XAie_LocType loc, u8 module, u8 event, void *arg);

private:
    // Core device to which the graph belongs.  The core device
    // has been loaded with an xclbin from which meta data can
    // be extracted
    std::shared_ptr<xrt_core::device> device;

    enum class graph_state : unsigned short
    {
      stop = 0,
      reset = 1,
      running = 2,
      suspend = 3,
      end = 4,
    };

    graph_state state;
    std::string name;
    uint64_t startTime;

    /**
     * This is the pointer to the AIE array where the AIE part of
     * the graph resides. The Aie is an obect that holds the whole
     * AIE resources, configurations etc.
     * TODO it should be initialized when we load XCLBIN?
     */
    Aie *aieArray;

    /**
     * This is the collections of tiles that this graph uses.
     * A tile is represented by a pair of number <col, row>
     * It represents the tile position in the AIE array.
     */
    std::vector<tile_type> tiles;

    /* This is the collections of rtps that are used. */
    std::vector<rtp_type> rtps;

    /* Wait for all the BD trasters for a given channel */
    void wait_sync_bo(ShimDMA *dmap, uint32_t chan, uint32_t timeout);
};

}

#endif
