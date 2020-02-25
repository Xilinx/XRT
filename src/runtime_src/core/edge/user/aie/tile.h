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

#ifndef _TILE_H_
#define _TILE_H_

#include "xclhal2.h"

namespace zynqaie {

class Tile {
public:
    ~Tile();
    Tile(int gRow, int gCol, int iRow, int iCol, uint32_t iMemAddr);

    int graphRow;
    int graphCol;
    int iterRow;
    int iterCol;

    uint32_t iterMemAddr;
};

}


#endif
