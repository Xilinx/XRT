/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_SOURCE

#include "xdp/profile/plugin/aie_trace/util/aie_trace_config.h"
#include "xdp/profile/plugin/aie_trace/util/aie_trace_util.h"
#include "xdp/profile/database/static_info/aie_util.h"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <regex>

#include "core/common/message.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp::aie::trace {
  using severity_level = xrt_core::message::severity_level;

  /****************************************************************************
   * Configure stream switch ports for monitoring purposes
   * NOTE: Used to monitor streams: trace, interfaces, and memory tiles
   ***************************************************************************/
  std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>>
  configStreamSwitchPorts(XAie_DevInst* aieDevInst, const tile_type& tile,
                          xaiefal::XAieTile& xaieTile, const XAie_LocType loc,
                          const module_type type, const std::string metricSet,
                          const uint8_t channel0, const uint8_t channel1, 
                          std::vector<XAie_Events>& events)
  {
    std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>> streamPorts;
    std::map<uint8_t, std::shared_ptr<xaiefal::XAieStreamPortSelect>> switchPortMap;

    // Traverse all counters and request monitor ports as needed
    for (int i=0; i < events.size(); ++i) {
      // Ensure applicable event
      auto event = events.at(i);
      if (!isStreamSwitchPortEvent(event))
        continue;

      bool newPort = false;
      auto portnum = getPortNumberFromEvent(event);

      // New port needed: reserver, configure, and store
      if (switchPortMap.find(portnum) == switchPortMap.end()) {
        auto switchPortRsc = xaieTile.sswitchPort();
        if (switchPortRsc->reserve() != AieRC::XAIE_OK)
          continue;
        newPort = true;
        switchPortMap[portnum] = switchPortRsc;

        if (type == module_type::core) {
          // AIE Tiles (e.g., trace streams)
          // Define stream switch port to monitor core or memory trace
          uint8_t traceSelect = (event == XAIE_EVENT_PORT_RUNNING_0_CORE) ? 0 : 1;
          std::string msg = "Configuring core module stream switch to monitor trace port " 
                          + std::to_string(traceSelect);
          xrt_core::message::send(severity_level::debug, "XRT", msg);
          switchPortRsc->setPortToSelect(XAIE_STRMSW_SLAVE, TRACE, traceSelect);
        }
        else if (type == module_type::shim) {
          // Interface tiles (e.g., PLIO, GMIO)
          // Grab slave/master and stream ID
          auto slaveOrMaster = (tile.itr_mem_col == 0) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
          std::string typeName = (tile.itr_mem_col == 0) ? "slave" : "master"; 
          auto streamPortId  = static_cast<uint8_t>(tile.itr_mem_row);
          std::string msg = "Configuring interface tile stream switch to monitor " 
                          + typeName + " stream port " + std::to_string(streamPortId);
          xrt_core::message::send(severity_level::debug, "XRT", msg);
          switchPortRsc->setPortToSelect(slaveOrMaster, SOUTH, streamPortId);
        }
        else {
          // Memory tiles
          if (metricSet.find("trace") != std::string::npos) {
            xrt_core::message::send(severity_level::debug, "XRT", 
              "Configuring memory tile stream switch to monitor trace port 0");
            switchPortRsc->setPortToSelect(XAIE_STRMSW_SLAVE, TRACE, 0);
          }
          else {
            uint8_t channel = (portnum == 0) ? channel0 : channel1;
            auto slaveOrMaster = isInputSet(type, metricSet) ? XAIE_STRMSW_MASTER : XAIE_STRMSW_SLAVE;
            std::string typeName = (slaveOrMaster == XAIE_STRMSW_MASTER) ? "master" : "slave";
            std::string msg = "Configuring memory tile stream switch to monitor " 
                            + typeName + " stream port " + std::to_string(channel);
            xrt_core::message::send(severity_level::debug, "XRT", msg);
            switchPortRsc->setPortToSelect(slaveOrMaster, DMA, channel);
          }
        }
      }

      auto switchPortRsc = switchPortMap[portnum];

      // Event options:
      //   getSSIdleEvent, getSSRunningEvent, getSSStalledEvent, & getSSTlastEvent
      XAie_Events ssEvent;
      if (isPortRunningEvent(event))
        switchPortRsc->getSSRunningEvent(ssEvent);
      else
        switchPortRsc->getSSStalledEvent(ssEvent);
      events.at(i) = ssEvent;

      if (newPort) {
        switchPortRsc->start();
        streamPorts.push_back(switchPortRsc);
      }
    }

    switchPortMap.clear();
    return streamPorts;
  }

  /****************************************************************************
   * Configure event selection (memory tiles only)
   ***************************************************************************/
  void configEventSelections(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                             const module_type type, const std::string metricSet, 
                             const uint8_t channel0, const uint8_t channel1)
  {
    if (type != module_type::mem_tile)
      return;

    XAie_DmaDirection dmaDir = isInputSet(type, metricSet) ? DMA_S2MM : DMA_MM2S;

    if (aie::isDebugVerbosity()) {
      std::string typeName = (dmaDir == DMA_S2MM) ? "S2MM" : "MM2S";
      std::string msg = "Configuring memory tile event selections to DMA " 
                      + typeName + " channels " + std::to_string(channel0) 
                      + " and " + std::to_string(channel1);
      xrt_core::message::send(severity_level::debug, "XRT", msg);
    }

    XAie_EventSelectDmaChannel(aieDevInst, loc, 0, dmaDir, channel0);
    XAie_EventSelectDmaChannel(aieDevInst, loc, 1, dmaDir, channel1);
  }

  /****************************************************************************
   * Configure edge detection events (memory tiles only)
   ***************************************************************************/
  void configEdgeEvents(XAie_DevInst* aieDevInst, const tile_type& tile,
                        const module_type type, const std::string metricSet, 
                        const XAie_Events event)
  {
    // For now, only memory tiles are supported
    if ((event != XAIE_EVENT_EDGE_DETECTION_EVENT_0_MEM_TILE)
        && (event != XAIE_EVENT_EDGE_DETECTION_EVENT_1_MEM_TILE))
      return;

    // AIE core register offsets
    constexpr uint64_t AIE_OFFSET_EDGE_CONTROL_MEM_TILE = 0x94408;

    // Event is DMA_S2MM_Sel0_stream_starvation or DMA_MM2S_Sel0_stalled_lock
    uint16_t eventNum = isInputSet(type, metricSet)
        ? EVENT_MEM_TILE_DMA_S2MM_SEL0_STREAM_STARVATION
        : EVENT_MEM_TILE_DMA_MM2S_SEL0_STALLED_LOCK;

    // Register Edge_Detection_event_control
    // 26    Event 1 triggered on falling edge
    // 25    Event 1 triggered on rising edge
    // 23:16 Input event for edge event 1
    // 10    Event 0 triggered on falling edge
    //  9    Event 0 triggered on rising edge
    //  7:0  Input event for edge event 0
    uint32_t edgeEventsValue = (1 << 26) + (eventNum << 16) + (1 << 9) + eventNum;

    std::string msg = "Configuring memory tile edge events to detect rise and fall of event " 
                    + std::to_string(eventNum);
    xrt_core::message::send(severity_level::debug, "XRT", msg);

    auto tileOffset = _XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
    XAie_Write32(aieDevInst, tileOffset + AIE_OFFSET_EDGE_CONTROL_MEM_TILE, edgeEventsValue);
  }

  /****************************************************************************
   * Configure delay for trace start event
   ***************************************************************************/
  bool configStartDelay(xaiefal::XAieMod& core, uint64_t delay,
                        XAie_Events& startEvent)
  {
    if (delay == 0)
      return false;

    // This algorithm daisy chains counters to get an effective 64 bit delay
    // counterLow -> counterHigh -> trace start
    uint32_t delayCyclesHigh = 0;
    uint32_t delayCyclesLow = 0;
    XAie_ModuleType mod = XAIE_CORE_MOD;
    bool useTwoCounters = (delay > std::numeric_limits<uint32_t>::max());

    if (useTwoCounters) {
      // ceil(x/y) where x and y are  positive integers
      delayCyclesHigh = static_cast<uint32_t>(1 + ((delay - 1) / std::numeric_limits<uint32_t>::max()));
      delayCyclesLow = static_cast<uint32_t>(delay / delayCyclesHigh);
    } else {
      delayCyclesLow = static_cast<uint32_t>(delay);
    }

    if (isDebugVerbosity()) {
      std::stringstream msg;
      msg << "Configuring AIE trace to start after delay of " << delay << " (low: " 
          << delayCyclesLow << ", high: " << delayCyclesHigh << ")" << std::endl;
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }

    // Configure lower 32 bits
    auto pc = core.perfCounter();
    if (pc->initialize(mod, XAIE_EVENT_ACTIVE_CORE, mod, XAIE_EVENT_DISABLED_CORE) != XAIE_OK)
      return false;
    if (pc->reserve() != XAIE_OK)
      return false;

    pc->changeThreshold(delayCyclesLow);
    
    XAie_Events counterEvent;
    pc->getCounterEvent(mod, counterEvent);
    // Reset when done counting
    pc->changeRstEvent(mod, counterEvent);
    if (pc->start() != XAIE_OK)
      return false;

    // Configure upper 32 bits if necessary
    // Use previous counter to start a new counter
    if (useTwoCounters && delayCyclesHigh) {
      auto pc = core.perfCounter();
      // Count by 1 when previous counter generates event
      if (pc->initialize(mod, counterEvent, mod, counterEvent) != XAIE_OK)
        return false;
      if (pc->reserve() != XAIE_OK)
        return false;
      pc->changeThreshold(delayCyclesHigh);
      pc->getCounterEvent(mod, counterEvent);
      // Reset when done counting
      pc->changeRstEvent(mod, counterEvent);
      if (pc->start() != XAIE_OK)
        return false;
    }

    startEvent = counterEvent;
    return true;
  }

  /****************************************************************************
   * Configure trace start on graph iteration
   ***************************************************************************/
  bool configStartIteration(xaiefal::XAieMod& core, uint32_t iteration,
                            XAie_Events& startEvent)
  {
    XAie_ModuleType mod = XAIE_CORE_MOD;
    // Count up by 1 for every iteration
    auto pc = core.perfCounter();
    if (pc->initialize(mod, XAIE_EVENT_INSTR_EVENT_0_CORE, 
                       mod, XAIE_EVENT_INSTR_EVENT_0_CORE) != XAIE_OK)
      return false;
    if (pc->reserve() != XAIE_OK)
      return false;

    xrt_core::message::send(severity_level::debug, "XRT", 
        "Configuring AIE trace to start on iteration " + std::to_string(iteration));

    pc->changeThreshold(iteration);
    
    XAie_Events counterEvent;
    pc->getCounterEvent(mod, counterEvent);
    // Reset when done counting
    pc->changeRstEvent(mod, counterEvent);
    if (pc->start() != XAIE_OK)
      return false;

    startEvent = counterEvent;
    return true;
  }

}  // namespace xdp
