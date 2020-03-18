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

    /* TODO is void good here? */
    (void) XAieGbl_CfgInitialize(&aieInst, tileArray.data(), aieConfigPtr);

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


