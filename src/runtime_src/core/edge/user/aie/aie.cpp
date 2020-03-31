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

#include "aie.h"

#include <iostream>
#include <cerrno>

namespace zynqaie {

Aie::Aie()
{
    /* TODO where are these number from */
    numRows = 8;
    numCols = 50;
    aieAddrArrayOff = 0x800;

    XAIEGBL_HWCFG_SET_CONFIG((&aieConfig), numRows, numCols, aieAddrArrayOff);
    XAieGbl_HwInit(&aieConfig);
    aieConfigPtr = XAieGbl_LookupConfig(XPAR_AIE_DEVICE_ID);

    int tileArraySize = numCols * (numRows + 1);
    tileArray.resize(tileArraySize);

    /*
     * Initialize AIE tile array.
     *
     * TODO is void good here?
     */
    (void) XAieGbl_CfgInitialize(&aieInst, tileArray.data(), aieConfigPtr);

    /*
     * Initialize graph GMIO metadata
     * TODO get gmio metadata from Xclbin
     */
    xrt_core::edge::aie::gmio_type gmio1;
    gmio1.id = "0";
    gmio1.name = "gmio1";
    gmio1.type = 0;
    gmio1.shim_col = 3;
    gmio1.channel_number = 2;
    gmios.emplace_back(gmio1);

    /*
     * Initialize AIE shim DMA on column base if there is one for
     * this column.
     */
    shim_dma.resize(numCols);
    for (auto& gmio : gmios) {
        auto dma = shim_dma.at(gmio.shim_col);
        auto pos = getTilePos(gmio.shim_col, 0);
        if (!dma.configured) {
            XAieDma_ShimSoftInitialize(&(tileArray.at(pos)), &(dma.handle));
            XAieDma_ShimBdClearAll(&(dma.handle));
            dma.configured = true;
        }

        auto chan = gmio.channel_number;
        XAieDma_ShimChControl((&(dma.handle)), chan, XAIE_DISABLE, XAIE_DISABLE, XAIE_ENABLE);
        for (int i = 0; i < XAIEGBL_NOC_DMASTA_STARTQ_MAX; ++i) {
            /*
             * 16 BDs are allocated to 4 channels.
             * Channel0: BD0~BD3
             * Channel1: BD4~BD7
             * Channel2: BD8~BD11
             * Channel3: BD12~BD15
             */
            int bd_num = chan * XAIEGBL_NOC_DMASTA_STARTQ_MAX + i;
            BD bd;
            bd.bd_num = bd_num;
            dma.dma_chan[chan].idle_bds.push(bd);

            XAieDma_ShimBdSetAxi(&(dma.handle), bd_num, 0, gmio.burst_len, 0, 0, 0);
        }
    }

#if 0
    /* TODO Register Event, Error XRT handler here ? */
    xaie_register_event_handler(event, module, xrtAieEventCallBack);
    xaie_register_error_notification(error, module, xrtAieErrorCallBack, arg);
#endif
}

Aie::~Aie()
{
#if 0
    /* TODO Unregister AIE Event, Error XRT handler. */
    xaie_unregister_events_notification();
    xaie_unregister_error_notification();
#endif
}

int Aie::getTilePos(int col, int row)
{
    return col * (numRows + 1) + row;
}

}
