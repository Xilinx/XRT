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

#include "adf_profiling_api.h"
#include "adf_runtime_api.h"
#include "fal_util.h"

extern "C"
{
#include "xaiengine.h"
}

#include <map>
#include <sstream>

namespace adf
{

static constexpr short INVALID_TILE_COORD = 0xFF;

XAie_Events COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[] = { XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_RUNNING_1_PL, XAIE_EVENT_PORT_RUNNING_2_PL, XAIE_EVENT_PORT_RUNNING_3_PL, XAIE_EVENT_PORT_RUNNING_4_PL, XAIE_EVENT_PORT_RUNNING_5_PL, XAIE_EVENT_PORT_RUNNING_6_PL, XAIE_EVENT_PORT_RUNNING_7_PL };

XAie_Events COMMON_XAIETILE_EVENT_SHIM_PORT_IDLE[] = { XAIE_EVENT_PORT_IDLE_0_PL, XAIE_EVENT_PORT_IDLE_1_PL, XAIE_EVENT_PORT_IDLE_2_PL, XAIE_EVENT_PORT_IDLE_3_PL, XAIE_EVENT_PORT_IDLE_4_PL, XAIE_EVENT_PORT_IDLE_5_PL, XAIE_EVENT_PORT_IDLE_6_PL, XAIE_EVENT_PORT_IDLE_7_PL };

XAie_Events COMMON_XAIETILE_EVENT_SHIM_BROADCAST_A[] = { XAIE_EVENT_BROADCAST_A_0_PL, XAIE_EVENT_BROADCAST_A_1_PL, XAIE_EVENT_BROADCAST_A_2_PL, XAIE_EVENT_BROADCAST_A_3_PL, XAIE_EVENT_BROADCAST_A_4_PL, XAIE_EVENT_BROADCAST_A_5_PL, XAIE_EVENT_BROADCAST_A_6_PL, XAIE_EVENT_BROADCAST_A_7_PL, XAIE_EVENT_BROADCAST_A_8_PL, XAIE_EVENT_BROADCAST_A_9_PL, XAIE_EVENT_BROADCAST_A_10_PL, XAIE_EVENT_BROADCAST_A_11_PL, XAIE_EVENT_BROADCAST_A_12_PL, XAIE_EVENT_BROADCAST_A_13_PL, XAIE_EVENT_BROADCAST_A_14_PL, XAIE_EVENT_BROADCAST_A_15_PL };

//////////////////////// shim_config ////////////////////////

shim_config::shim_config()
{
    shimColumn = -1;
    slaveOrMaster = 0;
    streamPortId = -1;
}

shim_config::shim_config(const gmio_config* pConfig)
{
    if (pConfig)
    {
        shimColumn = pConfig->shimColumn;
        pConfig->type == gmio_config::aie2gm ? slaveOrMaster = 1 : slaveOrMaster = 0;
        streamPortId = pConfig->streamId;
    }
    else
    {
        shimColumn = -1;
        slaveOrMaster = 0;
        streamPortId = -1;
    }
}

shim_config::shim_config(const plio_config* pConfig)
{
    if (pConfig)
    {
        shimColumn = pConfig->shimColumn;
        slaveOrMaster = pConfig->slaveOrMaster;
        streamPortId = pConfig->streamId;
    }
    else
    {
        shimColumn = -1;
        slaveOrMaster = 0;
        streamPortId = -1;
    }
}

//////////////////////// Profiling APIs ////////////////////////

err_code profiling::profile_stream_running_to_idle_cycles(XAie_DevInst* dev, shim_config shimConfig, std::vector<std::shared_ptr<xaiefal::XAieRsc>>& acquiredResources)
{
    int shimColumn = shimConfig.shimColumn;
    int streamPortId = shimConfig.streamPortId;
    u8 slaveOrMaster = shimConfig.slaveOrMaster; //0:slave, 1:master
    XAie_LocType tileLoc = XAie_TileLoc(shimColumn, 0);
    int driverStatus = AieRC::XAIE_OK; //0

    if (tileLoc.Row == INVALID_TILE_COORD || tileLoc.Col == INVALID_TILE_COORD || shimColumn < 0 || streamPortId < 0)
        return errorMsg(err_code::internal_error, "ERROR: event::start_profiling: Failed to access configuration information from IoAttr object.");

    auto pSSwitchPortRsc = fal_util::s_pXAieDev->tile(shimColumn, 0).sswitchPort();
    int eventPortId = fal_util::request(pSSwitchPortRsc);
    if (eventPortId < 0)
        return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request stream switch event port resources.");

    auto pPerfCounterRsc = fal_util::s_pXAieDev->tile(shimColumn, 0).pl().perfCounter();
    int counterId = fal_util::request(pPerfCounterRsc);
    if (counterId < 0)
        return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request performance counter resources.");

    debugMsg("event::io_total_stream_running_to_idle_cycles");

    driverStatus |= XAie_EventSelectStrmPort(dev, tileLoc, (u8)eventPortId, (slaveOrMaster == 0 ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER), SOUTH, (u8)streamPortId);
    debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_EventSelectStrmPort: col " << (int)tileLoc.Col
        << " row " << (int)tileLoc.Row << " , event port id " << (int)eventPortId << ", slave or master "
        << (int)slaveOrMaster << ", port interface SOUTH, stream switch port id " << (int)streamPortId).str());

    driverStatus |= XAie_PerfCounterControlSet(dev, tileLoc, XAIE_PL_MOD, (u8)counterId, COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId], COMMON_XAIETILE_EVENT_SHIM_PORT_IDLE[eventPortId]);
    debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterControlSet: col " << (int)tileLoc.Col
        << " row " << (int)tileLoc.Row << ", module XAIE_PL_MOD, counter id " << (int)counterId << ", start event COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING["
        << eventPortId << "], stop event COMMON_XAIETILE_EVENT_SHIM_PORT_IDLE[" << eventPortId << "]").str());

    acquiredResources = { pPerfCounterRsc, pSSwitchPortRsc }; //order of insertion matters, see profiling::read

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: event::start_profiling: AIE driver error.");

    return err_code::ok;
}

err_code profiling::profile_stream_start_to_transfer_complete_cycles(XAie_DevInst* dev, shim_config shimConfig, uint32_t numBytes, std::vector<std::shared_ptr<xaiefal::XAieRsc>>& acquiredResources)
{
    int shimColumn = shimConfig.shimColumn;
    int streamPortId = shimConfig.streamPortId;
    u8 slaveOrMaster = shimConfig.slaveOrMaster; //0:slave, 1:master
    XAie_LocType tileLoc = XAie_TileLoc(shimColumn, 0);
    int driverStatus = AieRC::XAIE_OK; //0

    if (tileLoc.Row == INVALID_TILE_COORD || tileLoc.Col == INVALID_TILE_COORD || shimColumn < 0 || streamPortId < 0)
        return errorMsg(err_code::internal_error, "ERROR: event::start_profiling: Failed to access configuration information from IoAttr object.");

    auto pSSwitchPortRsc = fal_util::s_pXAieDev->tile(shimColumn, 0).sswitchPort();
    int eventPortId = fal_util::request(pSSwitchPortRsc);
    if (eventPortId < 0)
        return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request stream switch event port resources.");

    auto pPerfCounterRsc0 = fal_util::s_pXAieDev->tile(shimColumn, 0).pl().perfCounter();
    int counterId0 = fal_util::request(pPerfCounterRsc0);
    if (counterId0 < 0)
        return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request performance counter resources.");

    auto pPerfCounterRsc1 = fal_util::s_pXAieDev->tile(shimColumn, 0).pl().perfCounter();
    int counterId1 = fal_util::request(pPerfCounterRsc1);
    if (counterId1 < 0)
        return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request performance counter resources.");

    debugMsg("event::io_stream_start_to_bytes_transferred_cycles");

    driverStatus |= XAie_EventSelectStrmPort(dev, tileLoc, (u8)eventPortId, (slaveOrMaster == 0 ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER), SOUTH, (u8)streamPortId);
    debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_EventSelectStrmPort: col " << (int)tileLoc.Col
        << " row " << (int)tileLoc.Row << " , event port id " << (int)eventPortId << ", slave or master " << (int)slaveOrMaster
        << ", port interface SOUTH, stream switch port id " << (int)streamPortId).str());

    driverStatus |= XAie_PerfCounterEventValueSet(dev, tileLoc, XAIE_PL_MOD, (u8)counterId1, (u32)(numBytes / 4));
    debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterEventValueSet: col " << (int)tileLoc.Col
        << " row " << (int)tileLoc.Row << ", module XAIE_PL_MOD, counter id " << (int)counterId1 << ", perf counter event value " << (unsigned int)(numBytes / 4)).str());

    driverStatus |= XAie_PerfCounterControlSet(dev, tileLoc, XAIE_PL_MOD, (u8)counterId0, COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId], XAIE_EVENT_PERF_CNT_1_PL);
    debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterControlSet: col " << (int)tileLoc.Col
        << " row " << (int)tileLoc.Row << ", module XAIE_PL_MOD, counter id " << (int)counterId0
        << ", start event COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[" << eventPortId << "], stop event XAIE_EVENT_PERF_CNT_1_PL ").str());

    driverStatus |= XAie_PerfCounterControlSet(dev, tileLoc, XAIE_PL_MOD, (u8)counterId1, COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId], COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId]);
    debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterControlSet: col " << (int)tileLoc.Col
        << " row " << (int)tileLoc.Row << ", module XAIE_PL_MOD, counter id " << (int)counterId1
        << ", start event COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[" << eventPortId
        << "], stop event COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[" << eventPortId << "]").str());

    acquiredResources = { pPerfCounterRsc0, pPerfCounterRsc1, pSSwitchPortRsc }; //order of insertion matters, see profiling::read

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: event::start_profiling: AIE driver error.");

    return err_code::ok;
}

err_code profiling::profile_stream_running_event_count(XAie_DevInst* dev, shim_config shimConfig, std::vector<std::shared_ptr<xaiefal::XAieRsc>>& acquiredResources)
{
    int shimColumn = shimConfig.shimColumn;
    int streamPortId = shimConfig.streamPortId;
    u8 slaveOrMaster = shimConfig.slaveOrMaster; //0:slave, 1:master
    XAie_LocType tileLoc = XAie_TileLoc(shimColumn, 0);
    int driverStatus = AieRC::XAIE_OK; //0

    if (tileLoc.Row == INVALID_TILE_COORD || tileLoc.Col == INVALID_TILE_COORD || shimColumn < 0 || streamPortId < 0)
        return errorMsg(err_code::internal_error, "ERROR: event::start_profiling: Failed to access configuration information from IoAttr object.");

    auto pSSwitchPortRsc = fal_util::s_pXAieDev->tile(shimColumn, 0).sswitchPort();
    int eventPortId = fal_util::request(pSSwitchPortRsc);
    if (eventPortId < 0)
        return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request stream switch event port resources.");

    auto pPerfCounterRsc = fal_util::s_pXAieDev->tile(shimColumn, 0).pl().perfCounter();
    int counterId = fal_util::request(pPerfCounterRsc);
    if (counterId < 0)
        return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request performance counter resources.");

    debugMsg("event::io_stream_running_event_count");

    driverStatus |= XAie_EventSelectStrmPort(dev, tileLoc, (u8)eventPortId, (slaveOrMaster == 0 ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER), SOUTH, (u8)streamPortId);
    debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_EventSelectStrmPort: col " << (int)tileLoc.Col
        << " row " << (int)tileLoc.Row << ", event port id " << (int)eventPortId << ", slave or master "
        << (int)slaveOrMaster << ", port interface SOUTH, stream switch port id " << (int)streamPortId).str());

    driverStatus |= XAie_PerfCounterControlSet(dev, tileLoc, XAIE_PL_MOD, (u8)counterId, COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId], COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId]);
    debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterControlSet: col " << (int)tileLoc.Col
        << " row " << (int)tileLoc.Row << ", module XAIE_PL_MOD, counter id " << (int)counterId
        << ", start event COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[" << eventPortId
        << "], stop event COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[" << eventPortId << "]").str());

    acquiredResources = { pPerfCounterRsc, pSSwitchPortRsc }; //order of insertion matters, see read_profiling

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: event::start_profiling: AIE driver error.");

    return err_code::ok;
}

err_code profiling::profile_start_time_difference_btw_two_streams(XAie_DevInst* dev, shim_config shimConfig1, shim_config shimConfig2, std::vector<std::shared_ptr<xaiefal::XAieRsc>>& acquiredResources)
{
    int shimColumn1 = shimConfig1.shimColumn;
    int streamPortId1 = shimConfig1.streamPortId;
    u8 slaveOrMaster1 = shimConfig1.slaveOrMaster; //0:slave, 1:master
    XAie_LocType tileLoc1 = XAie_TileLoc(shimColumn1, 0);

    int shimColumn2 = shimConfig2.shimColumn;
    int streamPortId2 = shimConfig2.streamPortId;
    u8 slaveOrMaster2 = shimConfig2.slaveOrMaster; //0:slave, 1:master
    XAie_LocType tileLoc2 = XAie_TileLoc(shimColumn2, 0);

    int driverStatus = AieRC::XAIE_OK; //0

    if (tileLoc1.Row == INVALID_TILE_COORD || tileLoc1.Col == INVALID_TILE_COORD || shimColumn1 < 0 || streamPortId1 < 0
        || tileLoc2.Row == INVALID_TILE_COORD || tileLoc2.Col == INVALID_TILE_COORD || shimColumn2 < 0 || streamPortId2 < 0)
        return errorMsg(err_code::internal_error, "ERROR: event::start_profiling: Failed to access configuration information from IoAttr object.");

    auto pSSwitchPortRsc1 = fal_util::s_pXAieDev->tile(shimColumn1, 0).sswitchPort();
    int eventPortId1 = fal_util::request(pSSwitchPortRsc1);
    if (eventPortId1 < 0)
        return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request stream switch event port resources.");

    auto pPerfCounterRsc1 = fal_util::s_pXAieDev->tile(shimColumn1, 0).pl().perfCounter();
    int counterId1 = fal_util::request(pPerfCounterRsc1);
    if (counterId1 < 0)
        return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request performance counter resources.");

    auto pSSwitchPortRsc2 = fal_util::s_pXAieDev->tile(shimColumn2, 0).sswitchPort();
    int eventPortId2 = fal_util::request(pSSwitchPortRsc2);
    if (eventPortId2 < 0)
        return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request stream switch event port resources.");

    auto pPerfCounterRsc2 = fal_util::s_pXAieDev->tile(shimColumn2, 0).pl().perfCounter();
    int counterId2 = fal_util::request(pPerfCounterRsc2);
    if (counterId2 < 0)
        return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request performance counter resources.");

    debugMsg("event::io_stream_start_difference_cycles");

    if (shimColumn1 == shimColumn2)
    {
        driverStatus |= XAie_EventSelectStrmPort(dev, tileLoc1, (u8)eventPortId1, (slaveOrMaster1 == 0 ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER), SOUTH, (u8)streamPortId1);
        debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_EventSelectStrmPort: col " << (int)tileLoc1.Col
            << " row " << (int)tileLoc1.Row << " , event port id " << (int)eventPortId1 << ", slave or master "
            << (int)slaveOrMaster1 << ", port interface SOUTH, stream switch port id " << (int)streamPortId1).str());

        driverStatus |= XAie_PerfCounterControlSet(dev, tileLoc1, XAIE_PL_MOD, (u8)counterId1, XAIE_EVENT_USER_EVENT_0_PL, COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId1]); //see Table 6-17
        debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterControlSet: col " << (int)tileLoc1.Col
            << " row " << (int)tileLoc1.Row << ", module XAIE_PL_MOD, counter id " << (int)counterId1
            << ", start event XAIE_EVENT_USER_EVENT_0_PL" << ", stop event COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[" << eventPortId1 << "]").str());

        driverStatus |= XAie_EventSelectStrmPort(dev, tileLoc2, (u8)eventPortId2, (slaveOrMaster2 == 0 ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER), SOUTH, (u8)streamPortId2);
        debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_EventSelectStrmPort: col " << (int)tileLoc2.Col
            << " row " << (int)tileLoc2.Row << " , event port id " << (int)eventPortId2 << ", slave or master " << (int)slaveOrMaster2
            << ", port interface SOUTH, stream switch port id " << (int)streamPortId2).str());

        driverStatus |= XAie_PerfCounterControlSet(dev, tileLoc2, XAIE_PL_MOD, (u8)counterId2, XAIE_EVENT_USER_EVENT_0_PL, COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId2]); //see Table 6-17
        debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterControlSet: col " << (int)tileLoc2.Col
            << " row " << (int)tileLoc2.Row << ", module XAIE_PL_MOD, counter id " << (int)counterId2
            << ", start event XAIE_EVENT_USER_EVENT_0_PL" << ", stop event COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[" << eventPortId2 << "]").str());

        driverStatus |= XAie_EventGenerate(dev, tileLoc1, XAIE_PL_MOD, XAIE_EVENT_USER_EVENT_0_PL);
        debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_EventGenerate: col " << (int)tileLoc1.Col
            << " row " << (int)tileLoc1.Row << ", module XAIE_PL_MOD, event XAIE_EVENT_USER_EVENT_0_PL").str());

        acquiredResources = { pPerfCounterRsc1, pPerfCounterRsc2, pSSwitchPortRsc1, pSSwitchPortRsc2 };
    }
    else
    {
        //if IO objects are located on different shim tiles, broadcast user event from shimColumn1 to shimColumn2,
        //reserve the broadcast resources for all shim tiles from shimColumn1 to shimColumn2
        //note that each shim tile pl module has broadcast switch A (handle internal events and the core or mem module on top of the shim)
        //and broadcast switch B (for relay purpose and handle the core or mem module on top of the shim)
        int westShimColumn = (shimColumn1 < shimColumn2) ? shimColumn1 : shimColumn2;
        int eastShimColumn = (shimColumn1 < shimColumn2) ? shimColumn2 : shimColumn1;
        int numBcastShimColumns = eastShimColumn - westShimColumn + 1;

        std::vector< XAie_LocType > vLocs(numBcastShimColumns);
        for (int i = 0; i < numBcastShimColumns; i++)
            vLocs[i] = XAie_TileLoc(westShimColumn + i, 0);

        //reserve broadcast channel along the shim tiles
        auto pBroadcastRsc = fal_util::s_pXAieDev->broadcast(vLocs, XAIE_PL_MOD, XAIE_PL_MOD);
        int broadcastId = fal_util::request(pBroadcastRsc);
        if (broadcastId < 0)
        {
            if (!fal_util::release(pPerfCounterRsc1) || !fal_util::release(pSSwitchPortRsc1) || !fal_util::release(pPerfCounterRsc2) || !fal_util::release(pSSwitchPortRsc2))
                errorMsg(err_code::aie_driver_error, "ERROR: event::start_profiling: Failed to release performance counter or stream switch event port resources.");

            return errorMsg(err_code::resource_unavailable, "ERROR: event::start_profiling: Failed to request event broadcast resources across shim tiles.");
        }

        //configure event stream port, performance counter, and event broadcast
        driverStatus |= XAie_EventSelectStrmPort(dev, tileLoc1, (u8)eventPortId1, (slaveOrMaster1 == 0 ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER), SOUTH, (u8)streamPortId1);
        debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_EventSelectStrmPort: col " << (int)tileLoc1.Col
            << " row " << (int)tileLoc1.Row << " , event port id " << (int)eventPortId1 << ", slave or master "
            << (int)slaveOrMaster1 << ", port interface SOUTH, stream switch port id " << (int)streamPortId1).str());

        driverStatus |= XAie_PerfCounterControlSet(dev, tileLoc1, XAIE_PL_MOD, (u8)counterId1, XAIE_EVENT_USER_EVENT_0_PL, COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId1]); //see Table 6-17
        debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterControlSet: col " << (int)tileLoc1.Col
            << " row " << (int)tileLoc1.Row << ", module XAIE_PL_MOD, counter id " << (int)counterId1
            << ", start event XAIE_EVENT_USER_EVENT_0_PL" << ", stop event COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[" << eventPortId1 << "]").str());

        driverStatus |= XAie_EventSelectStrmPort(dev, tileLoc2, (u8)eventPortId2, (slaveOrMaster2 == 0 ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER), SOUTH, (u8)streamPortId2);
        debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_EventSelectStrmPort: col " << (int)tileLoc2.Col
            << " row " << (int)tileLoc2.Row << " , event port id " << (int)eventPortId2 << ", slave or master " << (int)slaveOrMaster2
            << ", port interface SOUTH, stream switch port id " << (int)streamPortId2).str());

        driverStatus |= XAie_PerfCounterControlSet(dev, tileLoc2, XAIE_PL_MOD, (u8)counterId2, COMMON_XAIETILE_EVENT_SHIM_BROADCAST_A[broadcastId], COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId2]); //see Table 6-17
        debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterControlSet: col " << (int)tileLoc2.Col
            << " row " << (int)tileLoc2.Row << ", module XAIE_PL_MOD, counter id " << (int)counterId2 << ", start event COMMON_XAIETILE_EVENT_SHIM_BROADCAST_A["
            << broadcastId << "], stop event COMMON_XAIETILE_EVENT_SHIM_PORT_RUNNING[" << eventPortId2 << "]").str());

        //block event broadcast for unintended direction, see Figure 6-27 in Spec 1.5. In shim tile, only broadcast switch A connects to shim tile event generation, broadcast switch B is just a pure relay
        {
            u16 bcastMask = (1 << broadcastId);
            //west shim tile switch A
            XAie_LocType westTileLoc = XAie_TileLoc(westShimColumn, 0);
            driverStatus |= XAie_EventBroadcastBlockMapDir(dev, westTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, bcastMask, XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH); // block west, north & south
            driverStatus |= XAie_EventBroadcastBlockMapDir(dev, westTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, bcastMask, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH); // block north & south

            for (int i = 1; i < numBcastShimColumns - 1; i++)
            {
                XAie_LocType intermediateTileLoc = XAie_TileLoc(westShimColumn + i, 0);
                //intermediate shim tile switch A
                driverStatus |= XAie_EventBroadcastBlockMapDir(dev, intermediateTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, bcastMask, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH); // block north & south
                driverStatus |= XAie_EventBroadcastBlockMapDir(dev, intermediateTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, bcastMask, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH); // block north & south
            }

            //east shim tile switch A
            XAie_LocType eastTileLoc = XAie_TileLoc(eastShimColumn, 0);
            driverStatus |= XAie_EventBroadcastBlockMapDir(dev, eastTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, bcastMask, XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH); // block east, north & south
        }

        driverStatus |= XAie_EventBroadcast(dev, tileLoc1, XAIE_PL_MOD, (u8)broadcastId, XAIE_EVENT_USER_EVENT_0_PL);
        driverStatus |= XAie_EventGenerate(dev, tileLoc1, XAIE_PL_MOD, XAIE_EVENT_USER_EVENT_0_PL);

        acquiredResources = { pPerfCounterRsc1, pPerfCounterRsc2, pSSwitchPortRsc1, pSSwitchPortRsc2, pBroadcastRsc };
    }

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: event::start_profiling: AIE driver error.");

    return err_code::ok;
}

uint64_t profiling::read(XAie_DevInst* dev, std::vector<std::shared_ptr<xaiefal::XAieRsc>>& acquiredResources, bool startTimeDifference)
{
    uint64_t value = 0;
    int driverStatus = AieRC::XAIE_OK; //0

    if (startTimeDifference)
    {
        auto pPerfCounterRsc1 = dynamic_cast<xaiefal::XAiePerfCounter*>(acquiredResources[0].get());
        auto pPerfCounterRsc2 = dynamic_cast<xaiefal::XAiePerfCounter*>(acquiredResources[1].get());

        if (pPerfCounterRsc1 && pPerfCounterRsc2)
        {
            u32 value1, value2;
            uint32_t id1, id2;
            XAie_LocType loc1, loc2;
            XAie_ModuleType XAieModuleType1, XAieModuleType2;

            driverStatus |= pPerfCounterRsc1->getRscId(loc1, XAieModuleType1, id1);
            driverStatus |= pPerfCounterRsc2->getRscId(loc2, XAieModuleType2, id2);
            if (driverStatus != AieRC::XAIE_OK)
                errorMsg(err_code::aie_driver_error, "ERROR: event::read_profiling: Failed to get performance counter resource id");

            driverStatus |= XAie_PerfCounterGet(dev, loc1, XAieModuleType1, (u8)id1, &value1);
            debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterGet: col " << (int)loc1.Col <<
                " row " << (int)loc1.Row << ", module " << XAieModuleType1 << ", counterId " << id1).str());

            driverStatus |= XAie_PerfCounterGet(dev, loc2, XAieModuleType2, (u8)id2, &value2);
            debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterGet: col " << (int)loc2.Col <<
                " row " << (int)loc2.Row << ", module " << XAieModuleType2 << ", counterId " << id2).str());

            value = (long long)value2 - (long long)value1;

            //based on vcd file analysis
            //adjust 2 cycles per shim tile (broadcast-a, broadcast-b) broadcast propagation delay,
            //plus 1 cycle for the user event to trigger the broadcast signal in the source shim tile
            //plus 1 cycle for the ariving broadcast signal to trigger the broadcast event in the destination shim tile
            int shimColumn1 = loc1.Col; //shim column that initiates the broadcast
            int shimColumn2 = loc2.Col; //shim column that receives the broadcast
            if (shimColumn1 != shimColumn2)
            {
                int numBcastColumns = ((shimColumn2 > shimColumn1) ? (shimColumn2 - shimColumn1) : (shimColumn1 - shimColumn2));
                value += numBcastColumns * 2 + 2;
            }
        }
        else
            errorMsg(err_code::internal_error, "ERROR: event::read_profiling: The acquired resources order does not match the profiling option");
    }
    else
    {
        if (auto pPerfCounterRsc = dynamic_cast<xaiefal::XAiePerfCounter*>(acquiredResources[0].get()))
        {
            uint32_t id;
            XAie_LocType loc;
            XAie_ModuleType XAieModuleType;

            driverStatus |= pPerfCounterRsc->getRscId(loc, XAieModuleType, id);
            if (driverStatus != AieRC::XAIE_OK)
                errorMsg(err_code::aie_driver_error, "ERROR: event::read_profiling: Failed to get performance counter resource id");

            driverStatus |= XAie_PerfCounterGet(dev, loc, XAieModuleType, (u8)id, (u32*)(&value));
            debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterGet: col " << (int)loc.Col <<
                " row " << (int)loc.Row << ", module " << XAieModuleType << ", counterId " << id).str());
        }
        else
            errorMsg(err_code::internal_error, "ERROR: event::read_profiling: The acquired resources order does not match the profiling option");
    }

    return value;
}

err_code profiling::stop(XAie_DevInst* dev, std::vector<std::shared_ptr<xaiefal::XAieRsc>>& acquiredResources, bool startTimeDifference)
{
    int driverStatus = AieRC::XAIE_OK;
    debugMsg("event::stop_profiling");

    for (int i = 0; i < acquiredResources.size(); i++)
    {
        auto acquiredResource = acquiredResources[i];
        if (auto pSingleTileRsc = dynamic_cast<xaiefal::XAieSingleTileRsc*>(acquiredResource.get()))
        {
            uint32_t id;
            XAie_LocType loc;
            XAie_ModuleType XAieModuleType;

            driverStatus |= pSingleTileRsc->getRscId(loc, XAieModuleType, id);
            if (driverStatus != AieRC::XAIE_OK)
                return errorMsg(err_code::aie_driver_error, "ERROR: event::read_profiling: Failed to get resource id");

            if (auto pPerfCounterRsc = dynamic_cast<xaiefal::XAiePerfCounter*>(pSingleTileRsc))
            {
                driverStatus |= XAie_PerfCounterReset(dev, loc, XAieModuleType, (u8)id);
                debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterReset: col " << (int)loc.Col <<
                    " row " << (int)loc.Row << ", module " << XAieModuleType << ", counterId " << id).str());

                driverStatus |= XAie_PerfCounterResetControlReset(dev, loc, XAieModuleType, (u8)id);
                debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_PerfCounterResetControlReset: col " << (int)loc.Col <<
                    " row " << (int)loc.Row << ", module " << XAieModuleType << ", counterId " << id).str());
            }
            else if (auto pSSwitchPortEventRsc = dynamic_cast<xaiefal::XAieStreamPortSelect*>(pSingleTileRsc))
            {
                driverStatus |= XAie_EventSelectStrmPortReset(dev, loc, (u8)id);
                debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_EventSelectStrmPortReset: col " << (int)loc.Col
                    << " row " << (int)loc.Row << ", event port id " << id).str());
            }
            else if (auto pProgramCounterRsc = dynamic_cast<xaiefal::XAiePCEvent*>(pSingleTileRsc))
            {
                driverStatus |= XAie_EventPCReset(dev, loc, id);
                debugMsg(static_cast<std::stringstream &&>(std::stringstream() << "XAie_EventPCReset: col " << (int)loc.Col <<
                    " row " << (int)loc.Row << ", PCEventIndex " << id).str());
            }
        }
        else if (auto pBroadcastRsc = dynamic_cast<xaiefal::XAieBroadcast*>(acquiredResource.get()))
        {
            std::vector<XAie_LocType> tileLocs;
            XAie_ModuleType startModule, endModule;
            u8 broadcastId = (u8)pBroadcastRsc->getBc();
            pBroadcastRsc->getChannel(tileLocs, startModule, endModule);

            if (startTimeDifference && startModule == XAIE_PL_MOD && endModule == XAIE_PL_MOD)
            {
                //release shim pl module broadcast resources from source tile to destination tile

                //Step1 find the west shim id and east shim id across the broadcast.
                int shimColumn1 = tileLocs.front().Col; //shim column that initiates the broadcast
                int shimColumn2 = tileLocs.back().Col; //shim column that receives the broadcast
                int westShimColumn = (shimColumn1 < shimColumn2) ? shimColumn1 : shimColumn2;
                int eastShimColumn = (shimColumn1 < shimColumn2) ? shimColumn2 : shimColumn1;
                int numBcastShimColumns = eastShimColumn - westShimColumn + 1;

                driverStatus |= XAie_EventBroadcastReset(dev, XAie_TileLoc(shimColumn1, 0), XAIE_PL_MOD, broadcastId);

                XAie_LocType westTileLoc = XAie_TileLoc(westShimColumn, 0);

                //west shim tile switch A
                driverStatus |= XAie_EventBroadcastUnblockDir(dev, westTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, (u8)broadcastId, XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH); // unblock west, north & south

                //west shim tile switch B
                driverStatus |= XAie_EventBroadcastUnblockDir(dev, westTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, (u8)broadcastId, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH); // unblock north & south

                for (int i = 1; i < numBcastShimColumns - 1; i++)
                {
                    XAie_LocType intermediateTileLoc = XAie_TileLoc(westShimColumn + i, 0);

                    //intermediate shim tile switch A
                    driverStatus |= XAie_EventBroadcastUnblockDir(dev, intermediateTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, (u8)broadcastId, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH); // unblock north & south

                    //intermediate shim tile switch B
                    driverStatus |= XAie_EventBroadcastUnblockDir(dev, intermediateTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, (u8)broadcastId, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH); // unblock north & south
                }

                XAie_LocType eastTileLoc = XAie_TileLoc(eastShimColumn, 0);
                //east shim tile switch A
                driverStatus |= XAie_EventBroadcastUnblockDir(dev, eastTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, (u8)broadcastId, XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH); // unblock east, north & south
            }
        }

        if (!fal_util::release(acquiredResource))
        {
            errorMsg(err_code::aie_driver_error, "ERROR: event::stop_profiling: Failed to release acquired resources.");
            driverStatus |= XAIE_ERR;
        }
    }

    if (driverStatus != AieRC::XAIE_OK)
        return errorMsg(err_code::aie_driver_error, "ERROR: event::stop_profiling: AIE driver error.");

    return err_code::ok;
}

}
