/**
* Copyright (C) 2021 Xilinx, Inc
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

#pragma once

#include <string>
#include <vector>

namespace adf
{

struct driver_config
{
    uint8_t hw_gen;
    uint64_t base_address;
    uint8_t column_shift;
    uint8_t row_shift;
    uint8_t num_columns;
    uint8_t num_rows;
    uint8_t shim_row;
    uint8_t reserved_row_start;
    uint8_t reserved_num_rows;
    uint8_t aie_tile_row_start;
    uint8_t aie_tile_num_rows;
};

struct aiecompiler_options
{
    bool broadcast_enable_core;
};

struct graph_config
{
    int id;
    std::string name;

    std::vector<short> coreColumns;
    std::vector<short> coreRows;
    /// Core iteration memory address
    std::vector<short> iterMemColumns;
    std::vector<short> iterMemRows;
    std::vector<size_t> iterMemAddrs;
    std::vector<bool> triggered;
};

struct rtp_config
{
    int portId;
    int aliasId;
    std::string portName;
    std::string aliasName;
    int graphId;
    bool isInput;
    bool isAsync;
    bool isConnect;
    size_t numBytes;
    bool isPL;
    //for graph::update to connected async input RTP, if the connection is within a core, there may not be a lock
    bool hasLock;
    short selectorColumn;
    short selectorRow;
    size_t selectorAddr;
    unsigned short selectorLockId;
    short pingColumn;
    short pingRow;
    size_t pingAddr;
    unsigned short pingLockId;
    short pongColumn;
    short pongRow;
    size_t pongAddr;
    unsigned short pongLockId;
};

struct gmio_config
{
    enum gmio_type { gm2aie, aie2gm, gm2pl, pl2gm };

    /// GMIO object id
    int id;
    /// GMIO variable name
    std::string name;
    /// GMIO loginal name
    std::string logicalName;
    /// GMIO type
    gmio_type type;
    /// Shim tile column to where the GMIO is mapped
    short shimColumn;
    /// Channel number (0-S2MM0,1-S2MM1,2-MM2S0,3-MM2S1).
    short channelNum;
    /// Shim stream switch port id (slave: gm-->me, master: me-->gm)
    short streamId;
    /// For type == gm2aie or type == aie2gm, burstLength is the burst length for the AXI-MM transfer
    /// (4 or 8 or 16 in C_RTS API). The burst length in bytes is burstLength * 16 bytes (128-bit aligned).
    /// For type == gm2pl or type == pl2gm, burstLength is the burst length in bytes.
    short burstLength;
};

struct kernel_config
{
    ///Kernel object id
    int id;
    std::vector<int> hierarchicalGraphIds;
    short column;
    short row;
};

struct dma_config
{
    ///DMA object
    short column;
    short row;
    std::vector<int> hierarchicalGraphIds;
    std::vector<int> channel;
};

struct plio_config
{
    /// PLIO object id
    int id;
    /// PLIO variable name
    std::string name;
    /// PLIO loginal name
    std::string logicalName;
    /// Shim tile column to where the GMIO is mapped
    short shimColumn;
    /// slave or master. 0:slave, 1:master
    short slaveOrMaster;
    /// Shim stream switch port id
    short streamId;
};

struct trace_unit_config
{
    /// tile column
    short column;
    /// tile row
    short row;
    /// core module 0, memory module 1, shim pl module 2
    short module;
    /// packet id
    short packetId;
};

}
