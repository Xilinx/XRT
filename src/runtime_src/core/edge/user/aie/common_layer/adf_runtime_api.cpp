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

#include "adf_runtime_api.h"
#include "adf_api_message.h"

#include <algorithm>
#include <sstream>
#include <map>

extern "C"
{
#include "xaiengine.h"
}

namespace adf
{

/********************************* Statics & Constants *********************************/

static constexpr short INVALID_TILE_COORD = 0xFF;

static constexpr int ACQ_WRITE = 0;
static constexpr int ACQ_READ = 1;
static constexpr int REL_READ = 1;
static constexpr int REL_WRITE = 0;

static constexpr unsigned LOCK_TIMEOUT = 0x7FFFFFFF;


/********************************* config_manager *************************************/

XAie_DevInst* config_manager::s_pDevInst = nullptr;
bool config_manager::s_bInitialized = false;
size_t config_manager::s_num_reserved_rows = 0;
bool config_manager::s_broadcast_enable_core = false;

err_code config_manager::initialize(XAie_DevInst* devInst, size_t num_reserved_rows, bool broadcast_enable_core)
{
    if (!s_bInitialized)
    {
        if (!devInst)
            return errorMsg(err_code::internal_error, "ERROR: config_manager::initialize: Cannot initialize device instance.");

        s_pDevInst = devInst;
        s_num_reserved_rows = num_reserved_rows;
        s_broadcast_enable_core = broadcast_enable_core;
        s_bInitialized = true;
    }
    return err_code::ok;
}

/************************************ graph_api ************************************/

graph_api::graph_api(const graph_config* pConfig)
    : pGraphConfig(pConfig), isConfigured(false), isRunning(false), startTime(0)
{}

err_code graph_api::configure()
{
    if (!pGraphConfig)
        return errorMsg(err_code::internal_error, "ERROR: adf::graph_api::configure: Invalid graph configuration.");

    int numCores = pGraphConfig->coreColumns.size();
    if (pGraphConfig->coreRows.size() != numCores || pGraphConfig->iterMemAddrs.size() != numCores
        || pGraphConfig->triggered.size() != numCores || pGraphConfig->iterMemColumns.size() != numCores
        || pGraphConfig->iterMemRows.size() != numCores)
        return errorMsg(err_code::internal_error, "ERROR: adf::graph_api::configure: inconsistent number of cores.");

    coreTiles.resize(numCores);
    iterMemTiles.resize(numCores);
    for (int i = 0; i < numCores; i++)
    {
        size_t numReservedRows = config_manager::s_num_reserved_rows;
        coreTiles[i] = XAie_TileLoc(pGraphConfig->coreColumns[i], pGraphConfig->coreRows[i] + numReservedRows + 1);
        iterMemTiles[i] = XAie_TileLoc(pGraphConfig->iterMemColumns[i], pGraphConfig->iterMemRows[i] + numReservedRows + 1);
    }

    isConfigured = true;
    return err_code::ok;
}

err_code graph_api::run()
{
    if (!isConfigured)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::run: graph is not configured.");

    int driverStatus = AieRC::XAIE_OK; //0
    int numCores = coreTiles.size();

    // Record a snapshot of the graph cores startup/enable time
    if (numCores)
        driverStatus |= XAie_ReadTimer(config_manager::s_pDevInst, coreTiles[0], XAIE_CORE_MOD, (u64*)(&startTime));

    infoMsg("Enabling core(s) of graph " + pGraphConfig->name);

    if (config_manager::s_broadcast_enable_core)
    {
        XAie_StartTransaction(config_manager::s_pDevInst, XAIE_TRANSACTION_ENABLE_AUTO_FLUSH);
        for (int i = 0; i < numCores; i++)
        {
            //Set Enable_Event bits to 113
            XAie_Write32(config_manager::s_pDevInst, (_XAie_GetTileAddr(config_manager::s_pDevInst, coreTiles[i].Row, coreTiles[i].Col) + 0x00032008), 0x4472);
        }
        XAie_SubmitTransaction(config_manager::s_pDevInst, nullptr);

        //Trigger event 113 in shim_tile at column 0 by writing to Event_Generate
        XAie_EventGenerate(config_manager::s_pDevInst, XAie_TileLoc(0, 0), XAIE_PL_MOD, XAIE_EVENT_BROADCAST_A_6_PL);

        XAie_StartTransaction(config_manager::s_pDevInst, XAIE_TRANSACTION_ENABLE_AUTO_FLUSH);
        for (int i = 0; i < numCores; i++)
        {
            //Set Enable_Event bits to 0
            XAie_Write32(config_manager::s_pDevInst, (_XAie_GetTileAddr(config_manager::s_pDevInst, coreTiles[i].Row, coreTiles[i].Col) + 0x00032008), 0x4400);
        }
        XAie_SubmitTransaction(config_manager::s_pDevInst, nullptr);
    }
    else
    {
        XAie_StartTransaction(config_manager::s_pDevInst, XAIE_TRANSACTION_ENABLE_AUTO_FLUSH);
        for (int i = 0; i < numCores; i++)
            driverStatus |= XAie_CoreEnable(config_manager::s_pDevInst, coreTiles[i]);
        XAie_SubmitTransaction(config_manager::s_pDevInst, nullptr);
    }

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::run: AIE driver error.");

    isRunning = true;  // Set graph enable after enabling all cores
    return err_code::ok;
}

err_code graph_api::run(int iterations)
{
    if (!isConfigured)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::run: graph is not configured.");

    int driverStatus = AieRC::XAIE_OK; //0

    // Set iterations for the core(s) of graph
    infoMsg("Set iterations for the core(s) of graph " + pGraphConfig->name);

    int numCores = coreTiles.size();
    XAie_StartTransaction(config_manager::s_pDevInst, XAIE_TRANSACTION_ENABLE_AUTO_FLUSH);
    for (int i = 0; i < numCores; i++)
        driverStatus |= XAie_DataMemWrWord(config_manager::s_pDevInst, iterMemTiles[i], pGraphConfig->iterMemAddrs[i], (u32)iterations);
    XAie_SubmitTransaction(config_manager::s_pDevInst, nullptr);

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::run: AIE driver error.");

    return run();
}

err_code graph_api::wait()
{
    if (!isConfigured)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::wait: graph is not configured.");

    int driverStatus = AieRC::XAIE_OK; //0

    infoMsg("Waiting for core(s) of graph " + pGraphConfig->name + " to finish execution ...");

    int numCores = coreTiles.size();
    for (int i = 0; i < numCores; i++)
    {
        if (!pGraphConfig->triggered[i])
        {
            // Default timeout is 500us. The timeout is counted on AIE clock.
            // So even for a simple test-case this API call returns with error code XAIE_CORE_STATUS_TIMEOUT.
            while (XAie_CoreWaitForDone(config_manager::s_pDevInst, coreTiles[i], 0) == XAIE_CORE_STATUS_TIMEOUT) {}
            driverStatus |= XAie_CoreDisable(config_manager::s_pDevInst, coreTiles[i]);
        }
    }

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::wait: AIE driver error.");

    infoMsg("core(s) are done executing");

    isRunning = false;
    return err_code::ok;
}

err_code graph_api::wait(unsigned long long cycleTimeout)
{
    if (!isConfigured)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::wait: graph is not configured.");

    int driverStatus = AieRC::XAIE_OK; //0

    // CycleCnt has an upper limit of 0xFFFFFFFFFFFF or 300 trillion* cycles to prevent overflow
    if (cycleTimeout > 0xFFFFFFFFFFFF)
        return errorMsg(err_code::user_error, "ERROR: adf::graph::wait: Max cycle timeout value can be 0xFFFFFFFFFFFF.");

    infoMsg("Waiting for core(s) of graph " + pGraphConfig->name + " to complete " + std::to_string(cycleTimeout) + " cycles ...");

    int numCores = coreTiles.size();
    if (numCores)
    {
        // Adjust the cycle-timeout value
        unsigned long long elapsedTime;
        driverStatus |= XAie_ReadTimer(config_manager::s_pDevInst, coreTiles[0], XAIE_CORE_MOD, (u64*)(&elapsedTime));
        elapsedTime -= startTime;
        if (cycleTimeout > elapsedTime)
            driverStatus |= XAie_WaitCycles(config_manager::s_pDevInst, coreTiles[0], XAIE_CORE_MOD, (cycleTimeout - elapsedTime));
    }

    infoMsg("core(s) execution timed out");
    infoMsg("Disabling core(s) of graph " + pGraphConfig->name);

    for (int i = 0; i < numCores; i++)
        driverStatus |= XAie_CoreDisable (config_manager::s_pDevInst, coreTiles[i]);

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::wait: AIE driver error.");

    isRunning = false;
    return err_code::ok;
}

err_code graph_api::resume()
{
    if (!isConfigured)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::resume: graph is not configured.");

    int driverStatus = AieRC::XAIE_OK; //0

    infoMsg("Re-enabling unfinished core(s) of graph " + pGraphConfig->name);

    int numCores = coreTiles.size();
    if (numCores) // Reset the graph timer
        driverStatus |= XAie_ReadTimer(config_manager::s_pDevInst, coreTiles[0], XAIE_CORE_MOD, (u64*)(&startTime));

    for (int i = 0; i < numCores; i++)
    {
        bool isDone = false;
        driverStatus |= XAie_CoreReadDoneBit(config_manager::s_pDevInst, coreTiles[i], (u8*)(&isDone));
        if (!isDone)
            driverStatus |= XAie_CoreEnable (config_manager::s_pDevInst, coreTiles[i]); //Core Enable will clear Core_Done status bit
    }

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::resume: AIE driver error.");

    return err_code::ok;
}

err_code graph_api::end()
{
    if (!isConfigured)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::end: graph is not configured.");

    int driverStatus = AieRC::XAIE_OK; //0

    bool isRunningBefore = isRunning;
    err_code ret = wait(); //wait core done. //wait() sets isRunning to false
    if (ret != err_code::ok)
        return ret;

    int numCores = coreTiles.size();
    for (int i = 0; i < numCores; i++)
    {
        //if the end sequence is done before, do not do it again. this is to allow multiple g.end(), g.end() ...
        if (isRunningBefore && !pGraphConfig->triggered[i])
        {
            driverStatus |= XAie_DataMemWrWord(config_manager::s_pDevInst, iterMemTiles[i], pGraphConfig->iterMemAddrs[i] - 4, (u32)1);
            driverStatus |= XAie_CoreEnable(config_manager::s_pDevInst, coreTiles[i]);

            // Default timeout is 500us. The timeout is counted on AIE clock.
            // So even for a simple test-case this API call returns with error code XAIE_CORE_STATUS_TIMEOUT.
            while (XAie_CoreWaitForDone(config_manager::s_pDevInst, coreTiles[i], 0) == XAIE_CORE_STATUS_TIMEOUT) {}
            driverStatus |= XAie_CoreDisable(config_manager::s_pDevInst, coreTiles[i]);
        }
    }

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::end: AIE driver error.");

    return err_code::ok;
}

err_code graph_api::end(unsigned long long cycleTimeout)
{
    if (!isConfigured)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::end: graph is not configured.");

    int driverStatus = AieRC::XAIE_OK; //0

    err_code ret = wait(cycleTimeout);
    if (ret != err_code::ok)
        return ret;

    int numCores = iterMemTiles.size();
    //set the end signal in sync_buffer[0] (which is 4 byte before iteration address)
    for (int i = 0; i < numCores; i++)
        driverStatus |= XAie_DataMemWrWord(config_manager::s_pDevInst, iterMemTiles[i], pGraphConfig->iterMemAddrs[i] - 4, (u32)1);

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::end: AIE driver error.");

    return err_code::ok;
}

err_code checkRTPConfigForUpdate(const rtp_config* pRTPConfig, const graph_config* pGraphConfig, size_t numBytes, bool isRunning)
{
    if (!pRTPConfig)
        return errorMsg(err_code::internal_error, "ERROR: adf::graph::update: invalid RTP configuration.");

    //error checking: does this port belong to the graph
    if (pRTPConfig->graphId != pGraphConfig->id)
        return errorMsg(err_code::user_error, "ERROR: adf::graph::update: RTP port " + pRTPConfig->portName
            + " does not belong to graph " + pGraphConfig->name + ".");

    // error checking: direction
    if (!pRTPConfig->isInput)
        return errorMsg(err_code::user_error, "ERROR: adf::graph::update only supports input RTP port.");

    // error checking: size
    if (numBytes != pRTPConfig->numBytes)
        return errorMsg(err_code::user_error, "ERROR: adf::graph::update parameter size " + std::to_string(numBytes)
            + " is inconsistent with RTP port " + pRTPConfig->portName + " size " + std::to_string(pRTPConfig->numBytes) + ".");

    // error checking: connected RTP port
    if (pRTPConfig->isConnect)
    {
        if (pRTPConfig->isPL)
            return errorMsg(err_code::user_error, "ERROR: adf::graph::update to connected RL input RTP is not supported.");
        else //AIE RTP
        {
            // For connected async input RTP, only allow graph update before graph run
            if (pRTPConfig->isAsync)
            {
                if (isRunning)
                    return errorMsg(err_code::user_error, "ERROR: adf::graph::update to connected asynchronous input RTP is not allowed during graph run.");
            }
            // Does not support connected sync input RTP
            else
                return errorMsg(err_code::user_error, "ERROR: adf::graph::update to connected synchronous input RTP is not supported.");
        }
    }

    return err_code::ok;
}

err_code graph_api::update(const rtp_config* pRTPConfig, const void* pValue, size_t numBytes)
{
    ///////////////////////////// Error Checking //////////////////////////////

    err_code ret = checkRTPConfigForUpdate(pRTPConfig, pGraphConfig, numBytes, isRunning);
    if (ret != err_code::ok)
        return ret;

    ///////////////////////////// Configuration //////////////////////////////

    size_t numReservedRows = config_manager::s_num_reserved_rows;
    XAie_LocType selectorTile = XAie_TileLoc(pRTPConfig->selectorColumn, pRTPConfig->selectorRow + numReservedRows + 1);
    XAie_LocType pingTile = XAie_TileLoc(pRTPConfig->pingColumn, pRTPConfig->pingRow + numReservedRows + 1);
    XAie_LocType pongTile = XAie_TileLoc(pRTPConfig->pongColumn, pRTPConfig->pongRow + numReservedRows + 1);

    // Do NOT lock async RTP when graph is suspended; otherwise, it may deadlock. We don't support synchronous RTP in suspended mode
    bool bAcquireLock = !(pRTPConfig->isAsync && !isRunning);

    int8_t acquireVal = (pRTPConfig->isAsync ? XAIE_LOCK_WITH_NO_VALUE : ACQ_WRITE); //Versal
    int8_t releaseVal = REL_READ; //Versal

    ///////////////////////////// RTP update operation //////////////////////////////

    infoMsg("Updating RTP value to port " + pRTPConfig->portName);

    int driverStatus = AieRC::XAIE_OK; //0

    // sync ports acquire selector lock for WRITE, async ports acquire selector lock unconditionally
    if (pRTPConfig->hasLock && bAcquireLock)
        driverStatus |= XAie_LockAcquire(config_manager::s_pDevInst, selectorTile, XAie_LockInit(pRTPConfig->selectorLockId, acquireVal), LOCK_TIMEOUT);

    // Read the selector value
    u32 selector;
    driverStatus |= XAie_DataMemRdWord(config_manager::s_pDevInst, selectorTile, pRTPConfig->selectorAddr, ((u32*)&selector));
    selector = 1 - selector;

    if (selector == 1) //pong
    {
        // sync ports acquire buffer lock for WRITE, async ports acquire buffer lock unconditionally
        if (pRTPConfig->hasLock && bAcquireLock)
            driverStatus |= XAie_LockAcquire(config_manager::s_pDevInst, pongTile, XAie_LockInit(pRTPConfig->pongLockId, acquireVal), LOCK_TIMEOUT);

        driverStatus |= XAie_DataMemBlockWrite(config_manager::s_pDevInst, pongTile, pRTPConfig->pongAddr, const_cast<void*>(pValue), numBytes);
    }
    else //ping
    {
        // sync ports acquire buffer lock for WRITE, async ports acquire buffer lock unconditionally
        if (pRTPConfig->hasLock && bAcquireLock)
            driverStatus |= XAie_LockAcquire(config_manager::s_pDevInst, pingTile, XAie_LockInit(pRTPConfig->pingLockId, acquireVal), LOCK_TIMEOUT);

        driverStatus |= XAie_DataMemBlockWrite(config_manager::s_pDevInst, pingTile, pRTPConfig->pingAddr, const_cast<void*>(pValue), numBytes);
    }

    // write the new selector value
    driverStatus |= XAie_DataMemWrWord(config_manager::s_pDevInst, selectorTile, pRTPConfig->selectorAddr, selector);

    if (pRTPConfig->hasLock)
    {
        // release selector and buffer locks for ME
        // still need to release async RTP selector lock FOR_READ even when the graph is suspended;
        // otherwise, the ME side may deadlock in acquiring selector lock FOR_READ
        driverStatus |= XAie_LockRelease(config_manager::s_pDevInst, selectorTile, XAie_LockInit(pRTPConfig->selectorLockId, releaseVal), LOCK_TIMEOUT);

        // still need to release async RTP buffer lock FOR_READ even when the graph is suspended;
        // otherwise, the AIE side may deadlock in acquiring buffer lock FOR_READ
        // (note that there is one selector lock but two buffer locks)
        if (selector == 1) //pong
            driverStatus |= XAie_LockRelease(config_manager::s_pDevInst, pongTile, XAie_LockInit(pRTPConfig->pongLockId, releaseVal), LOCK_TIMEOUT);
        else //ping
            driverStatus |= XAie_LockRelease(config_manager::s_pDevInst, pingTile, XAie_LockInit(pRTPConfig->pingLockId, releaseVal), LOCK_TIMEOUT);
    }

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::update: XAieTile_LockAcquire timeout or AIE driver error.");

    return err_code::ok;
}

err_code checkRTPConfigForRead(const rtp_config* pRTPConfig, const graph_config* pGraphConfig, size_t numBytes)
{
    if (!pRTPConfig)
        return errorMsg(err_code::internal_error, "ERROR: adf::graph::read: Invalid RTP configuration.");

    //error checking: does this port belong to the graph
    if (pRTPConfig->graphId != pGraphConfig->id)
        return errorMsg(err_code::user_error, "ERROR: adf::graph::read: RTP port " + pRTPConfig->portName
            + " does not belong to graph " + pGraphConfig->name + ".");

    // error checking: direction
    if (pRTPConfig->isInput)
        return errorMsg(err_code::user_error, "ERROR: adf::graph::read does not support input RTP port.");

    // error checking: size
    if (numBytes != pRTPConfig->numBytes)
        return errorMsg(err_code::user_error, "ERROR: adf::graph::read parameter size " + std::to_string(numBytes)
            + " is inconsistent with RTP port " + pRTPConfig->portName + " size " + std::to_string(pRTPConfig->numBytes) + ".");

    // error checking: connected RTP port
    if (pRTPConfig->isConnect)
        return errorMsg(err_code::user_error, "ERROR: adf::graph::read from connected RTP port is not supported.");

    return err_code::ok;
}

err_code graph_api::read(const rtp_config* pRTPConfig, void* pValue, size_t numBytes)
{
    ///////////////////////////// Error Checking //////////////////////////////

    err_code ret = checkRTPConfigForRead(pRTPConfig, pGraphConfig, numBytes);
    if (ret != err_code::ok)
        return ret;

    ///////////////////////////// Configuration //////////////////////////////

    // Do NOT lock async RTP when graph is suspended; otherwise, it may deadlock. We don't support synchronous RTP in suspended mode
    bool bHasAndAcquireLock = !(pRTPConfig->isAsync && !isRunning) && pRTPConfig->hasLock;

    int8_t acquireVal = ACQ_READ; //Versal
    int8_t releaseVal = (pRTPConfig->isAsync ? REL_READ : REL_WRITE); //Versal

    size_t numReservedRows = config_manager::s_num_reserved_rows;
    XAie_LocType selectorTile = XAie_TileLoc(pRTPConfig->selectorColumn, pRTPConfig->selectorRow + numReservedRows + 1);
    XAie_LocType pingTile = XAie_TileLoc(pRTPConfig->pingColumn, pRTPConfig->pingRow + numReservedRows + 1);
    XAie_LocType pongTile = XAie_TileLoc(pRTPConfig->pongColumn, pRTPConfig->pongRow + numReservedRows + 1);

    ///////////////////////////// RTP read operation //////////////////////////////

    infoMsg("Reading RTP value from port " + pRTPConfig->portName);

    int driverStatus = AieRC::XAIE_OK; //0

    if (bHasAndAcquireLock)
    {
        // synchronous RTP acquires lock for READ, async RTP requiring first-time sync acquires lock for READ
        driverStatus |= XAie_LockAcquire(config_manager::s_pDevInst, selectorTile, XAie_LockInit(pRTPConfig->selectorLockId, acquireVal), LOCK_TIMEOUT);
    }

    // Read the selector value
    u32 selector;
    driverStatus |= XAie_DataMemRdWord(config_manager::s_pDevInst, selectorTile, pRTPConfig->selectorAddr, ((u32*)&selector));

    if (bHasAndAcquireLock)
    {
        // synchronous RTP acquires buffer for READ, async RTP requiring first-time sync acquires lock for READ
        if (selector == 1) //pong
            driverStatus |= XAie_LockAcquire(config_manager::s_pDevInst, pongTile, XAie_LockInit(pRTPConfig->pongLockId, acquireVal), LOCK_TIMEOUT);
        else //ping
            driverStatus |= XAie_LockAcquire(config_manager::s_pDevInst, pingTile, XAie_LockInit(pRTPConfig->pingLockId, acquireVal), LOCK_TIMEOUT);
    }

    //if lock was aquired, release the selector lock
    if (bHasAndAcquireLock)
    {
        // synchronous RTP releases lock for WRITE, async RTP requiring first-time sync release lock for READ
        driverStatus |= XAie_LockRelease(config_manager::s_pDevInst, selectorTile, XAie_LockInit(pRTPConfig->selectorLockId, releaseVal), LOCK_TIMEOUT);
    }

    if (selector == 1) //pong
        driverStatus |= XAie_DataMemBlockRead(config_manager::s_pDevInst, pongTile, pRTPConfig->pongAddr, pValue, numBytes);
    else //ping
        driverStatus |= XAie_DataMemBlockRead(config_manager::s_pDevInst, pingTile, pRTPConfig->pingAddr, pValue, numBytes);

    //release buffer lock
    if (bHasAndAcquireLock)
    {
        if (selector == 1) //pong
            driverStatus |= XAie_LockRelease(config_manager::s_pDevInst, pongTile, XAie_LockInit(pRTPConfig->pongLockId, releaseVal), LOCK_TIMEOUT);
        else //ping
            driverStatus |= XAie_LockRelease(config_manager::s_pDevInst, pingTile, XAie_LockInit(pRTPConfig->pingLockId, releaseVal), LOCK_TIMEOUT);
    }

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::graph::read: XAieTile_LockAcquire timeout or AIE driver error.");

     return err_code::ok;
}


/************************************ gmio_api ************************************/

/// GMIO API helper functions
static inline u8 convertLogicalToPhysicalDMAChNum(short logicalChNum)
{
    return (logicalChNum > 1 ? (logicalChNum - 2) : logicalChNum);
}

size_t frontAndPop(std::queue<size_t>& bdQueue)
{
    size_t bd = bdQueue.front();
    bdQueue.pop();
    return bd;
}

gmio_api::gmio_api(const gmio_config* pConfig) : pGMIOConfig(pConfig), isConfigured(false), dmaStartQMaxSize(4)
{}

err_code gmio_api::configure()
{
    if (!pGMIOConfig)
        return errorMsg(err_code::internal_error, "ERROR: gmio_api::configure: Invalid GMIO configuration.");

    if (pGMIOConfig->type == gmio_config::gm2aie || pGMIOConfig->type == gmio_config::aie2gm)
    {
        int driverStatus = AieRC::XAIE_OK; //0
        gmioTileLoc = XAie_TileLoc(pGMIOConfig->shimColumn, 0);
        driverStatus |= XAie_DmaDescInit(config_manager::s_pDevInst, &shimDmaInst, gmioTileLoc);
        //enable shim DMA channel, need to start first so the status is correct
        driverStatus |= XAie_DmaChannelEnable(config_manager::s_pDevInst, gmioTileLoc, convertLogicalToPhysicalDMAChNum(pGMIOConfig->channelNum), (pGMIOConfig->type == gmio_config::gm2aie ? DMA_MM2S : DMA_S2MM));
        driverStatus |= XAie_DmaGetMaxQueueSize(config_manager::s_pDevInst, gmioTileLoc, &dmaStartQMaxSize);

        //decide 4 BD numbers to use for this GMIO based on channel number (0-S2MM0,1-S2MM1,2-MM2S0,3-MM2S1)
        for (int j = 0; j < dmaStartQMaxSize; j++)
        {
            int bdNum = pGMIOConfig->channelNum * dmaStartQMaxSize + j;
            availableBDs.push(bdNum);

            //set AXI burst length, this won't change during runtime
            driverStatus |= XAie_DmaSetAxi(&shimDmaInst, 0 /*Smid*/, pGMIOConfig->burstLength /*BurstLen*/, 0 /*Qos*/, 0 /*Cache*/, 0 /*Secure*/);
            debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "GMIO id " << pGMIOConfig->id << " assigned BD num " << bdNum).str());
        }

        if (driverStatus != AieRC::XAIE_OK)
            return errorMsg(err_code::aie_driver_error, "ERROR: adf::gmio_api::configure: AIE driver error.");
    }
    else
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::gmio_api::configure: GM - PL connection is not supported in GMIO AIE API.");

    isConfigured = true;
    return err_code::ok;
}

err_code gmio_api::enqueueBD(uint64_t address, size_t size)
{
    if (!isConfigured)
        return errorMsg(err_code::internal_error, "ERROR: adf::gmio_api::enqueueBD: GMIO is not configured.");

    int driverStatus = XAIE_OK; //0

    //wait for available BD
    while (availableBDs.empty())
    {
        u8 numPendingBDs = 0;
        driverStatus |= XAie_DmaGetPendingBdCount(config_manager::s_pDevInst, gmioTileLoc, convertLogicalToPhysicalDMAChNum(pGMIOConfig->channelNum), (pGMIOConfig->type == gmio_config::gm2aie ? DMA_MM2S : DMA_S2MM), &numPendingBDs);

        int numBDCompleted = dmaStartQMaxSize - numPendingBDs;
        //move completed BDs from enqueuedBDs to availableBDs
        for (int i = 0; i < numBDCompleted; i++)
        {
            size_t bdNumber = frontAndPop(enqueuedBDs);
            availableBDs.push(bdNumber);
        }
    }

    //get an available BD
    size_t bdNumber = frontAndPop(availableBDs);

    //set up BD
    driverStatus |= XAie_DmaSetAddrLen(&shimDmaInst, (u64)address, (u32)size);
    driverStatus |= XAie_DmaSetLock(&shimDmaInst, XAie_LockInit(bdNumber, XAIE_LOCK_WITH_NO_VALUE), XAie_LockInit(bdNumber, XAIE_LOCK_WITH_NO_VALUE));
    driverStatus |= XAie_DmaEnableBd(&shimDmaInst);

    //write BD
    driverStatus |= XAie_DmaWriteBd(config_manager::s_pDevInst, &shimDmaInst, gmioTileLoc, bdNumber);

    //enqueue BD
    driverStatus |= XAie_DmaChannelPushBdToQueue(config_manager::s_pDevInst, gmioTileLoc, convertLogicalToPhysicalDMAChNum(pGMIOConfig->channelNum), (pGMIOConfig->type == gmio_config::gm2aie ? DMA_MM2S : DMA_S2MM), bdNumber);
    enqueuedBDs.push(bdNumber);

    debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "gmio_api::enqueueBD: (id "
        << pGMIOConfig->id << ") enqueue BD num " << bdNumber << " to shim DMA channel " << pGMIOConfig->channelNum
        << ", DDR address " << std::hex << address << ", transaction size " << std::dec << size).str());

    // Update status after using AIE driver
    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: adf::gmio_api::enqueueBD: AIE driver error.");

    return err_code::ok;
}

err_code gmio_api::wait()
{
    if (!isConfigured)
        return errorMsg(err_code::internal_error, "ERROR: adf::gmio_api::enqueueBD: GMIO is not configured.");

    if (pGMIOConfig->type == gmio_config::gm2pl || pGMIOConfig->type == gmio_config::pl2gm)
        return errorMsg(err_code::user_error, "ERROR: GMIO::wait can only be used by GMIO objects connecting to AIE, not PL.");

    debugMsg("gmio_api::wait::XAie_DmaWaitForDone ...");

    while (XAie_DmaWaitForDone(config_manager::s_pDevInst, gmioTileLoc, convertLogicalToPhysicalDMAChNum(pGMIOConfig->channelNum), (pGMIOConfig->type == gmio_config::gm2aie ? DMA_MM2S : DMA_S2MM), 0) != XAIE_OK) {}

    while (!enqueuedBDs.empty())
    {
        size_t bdNumber = frontAndPop(enqueuedBDs);
        availableBDs.push(bdNumber);
    }

    return err_code::ok;
}

}
