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

#include "errno.h"
#include "adf_api_config.h"

#include <string>
#include <queue>

extern "C"
{
    #include "xaiengine/xaiegbl.h"
}

namespace adf
{

enum class err_code : int
{
    ok = 0,
    user_error = EINVAL,
    internal_error = ENOTSUP,
    aie_driver_error = EIO
};

class config_manager
{
public:
    static err_code initialize(XAie_DevInst* devInst, size_t num_reserved_rows, bool broadcast_enable_core);

    static XAie_DevInst* s_pDevInst;

    static bool s_bInitialized;
    static size_t s_num_reserved_rows;
    static bool s_broadcast_enable_core;
};


class graph_api
{
public:
    graph_api(const graph_config* pConfig);
    virtual ~graph_api() {}

    err_code configure();
    err_code run();
    err_code run(int testIter);
    err_code wait();
    err_code wait(unsigned long long cycleTimeout);
    err_code resume();
    err_code end();
    err_code end(unsigned long long cycleTimeout);
    err_code update(const rtp_config* pRTPConfig, const void* pValue, size_t numBytes);
    err_code read(const rtp_config* pRTPConfig, void* pValue, size_t numBytes);

private:
    const graph_config* pGraphConfig;
    bool isConfigured;
    bool isRunning;
    unsigned long long startTime;

    std::vector<XAie_LocType> coreTiles;
    std::vector<XAie_LocType> iterMemTiles;
};

class gmio_api
{
public:
    gmio_api(const gmio_config* pConfig);
    virtual ~gmio_api() {}

    err_code configure();
    err_code enqueueBD(uint64_t address, size_t size);
    err_code wait();

private:
    /// GMIO shim DMA physical configuration compiled by the AIE compiler
    const gmio_config* pGMIOConfig;

    /// C_RTS Shim DMA to where this GMIO object is mapped
    XAie_DmaDesc shimDmaInst;
    XAie_LocType gmioTileLoc;

    bool isConfigured;
    uint8_t dmaStartQMaxSize;
    std::queue<size_t> enqueuedBDs;
    std::queue<size_t> availableBDs;
};

err_code checkRTPConfigForUpdate(const rtp_config* pRTPConfig, const graph_config* pGraphConfig, size_t numBytes, bool isRunning = false);
err_code checkRTPConfigForRead(const rtp_config* pRTPConfig, const graph_config* pGraphConfig, size_t numBytes);

}
