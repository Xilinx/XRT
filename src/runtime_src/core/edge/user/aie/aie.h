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

extern "C" {
#include <xaiengine.h>
}

namespace zynqaie {

class Aie {
public:
    ~Aie();
    Aie();

    std::vector<XAieGbl_Tile> tileArray;  // Tile Array

    int getTilePos(int col, int row);

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
