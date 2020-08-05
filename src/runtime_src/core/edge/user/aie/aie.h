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

#ifndef xrt_core_edge_user_aie_h
#define xrt_core_edge_user_aie_h

#include <vector>
#include <queue>
#include <memory>

#include "core/common/device.h"
#include "core/edge/common/aie_parser.h"
extern "C" {
#include <xaiengine.h>
}

namespace zynqaie {

struct BD {
    uint16_t bd_num;
    uint16_t addr_high;
    uint32_t addr_low;
};

struct DMAChannel {
    std::queue<BD> idle_bds;
    std::queue<BD> pend_bds;
};

struct ShimDMA {
    XAieDma_Shim handle;
    DMAChannel dma_chan[XAIEDMA_SHIM_MAX_NUM_CHANNELS];
    bool configured;
};


class Aie {
public:
    using gmio_type = xrt_core::edge::aie::gmio_type;

    ~Aie();
    Aie(std::shared_ptr<xrt_core::device> device);

    std::vector<XAieGbl_Tile> tileArray;  // Tile Array
    std::vector<ShimDMA> shim_dma;   // shim DMA

    /* This is the collections of gmios that are used. */
    std::vector<gmio_type> gmios;

    int getTilePos(int col, int row);

    XAieGbl *getAieInst();

    static XAieGbl_ErrorHandleStatus
    error_cb(struct XAieGbl *aie_inst, XAie_LocType loc, u8 module, u8 error, void *arg);

private:
    int numRows;
    int numCols;
    uint64_t aieAddrArrayOff;

    XAieGbl_Config *aieConfigPtr; // AIE configuration pointer
    XAieGbl aieInst;              // AIE global instance
    XAieGbl_HwCfg aieConfig;      // AIE configuration pointer
};

}

#endif
