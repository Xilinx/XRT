/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

/*
 * Copyright (C) 2015 Xilinx, Inc
 * Author: Paul Schumacher
 * Performance Monitoring using PCIe for XDMA HAL Driver
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

#include "core/include/xdp/counters.h"
#include "core/include/xdp/aim.h"
// Local/XRT headers
#include "config.h"
#include "shim.h"
#include "xrt/detail/xclbin.h"
// C++ headers
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifndef _WINDOWS
// TODO: Windows build support
//    unistd.h is linux only header file
//    it is included for read, write, close, lseek64
#include <unistd.h>
#endif

#ifdef _WINDOWS
#define __func__ __FUNCTION__
#endif

namespace xclhwemhal2
{
  const std::vector<std::string> kSimProcessStatus = {"SIM-IPC's external process can be connected to instance",
                                              "SystemC TLM functional mode",
                                              "HLS_PRINT",
                                              "Exiting xsim",
                                              "FATAL_ERROR"};

  constexpr auto kMaxTimeToConnectSimulator = 300;  // in seconds

  void HwEmShim::readDebugIpLayout(const std::string debugFileName)
  {
    //
    // Profiling - addresses and names
    // Parsed from debug_ip_layout.rtd contained in xclbin
    if (mLogStream.is_open())
    {
      mLogStream << "debug_ip_layout: reading profile addresses and names..." << std::endl;
    }

    memset(mPerfmonProperties, 0, sizeof(uint8_t) * xdp::MAX_NUM_AIMS);
    memset(mAccelmonProperties, 0, sizeof(uint8_t) * xdp::MAX_NUM_AMS);
    memset(mStreamMonProperties, 0, sizeof(uint8_t) * xdp::MAX_NUM_ASMS);

    mMemoryProfilingNumberSlots = getIPCountAddrNames(debugFileName, AXI_MM_MONITOR, mPerfMonBaseAddress,
                                                      mPerfMonSlotName, mPerfmonProperties, xdp::MAX_NUM_AIMS);

    mAccelProfilingNumberSlots = getIPCountAddrNames(debugFileName, ACCEL_MONITOR, mAccelMonBaseAddress,
                                                     mAccelMonSlotName, mAccelmonProperties, xdp::MAX_NUM_AMS);

    mStreamProfilingNumberSlots = getIPCountAddrNames(debugFileName, AXI_STREAM_MONITOR, mStreamMonBaseAddress,
                                                      mStreamMonSlotName, mStreamMonProperties, xdp::MAX_NUM_ASMS);

    mIsDeviceProfiling = (mMemoryProfilingNumberSlots > 0 || mAccelProfilingNumberSlots > 0 || mStreamProfilingNumberSlots > 0);

    std::string fifoName;
    uint64_t fifoCtrlBaseAddr = 0x0;
    uint32_t fifoCtrlCount = getIPCountAddrNames(debugFileName, AXI_MONITOR_FIFO_LITE, &fifoCtrlBaseAddr, &fifoName, nullptr, 1);
    mPerfMonFifoCtrlBaseAddress = fifoCtrlBaseAddr;

    uint64_t fifoReadBaseAddr = 0x0;
    uint32_t fifoFullCount = getIPCountAddrNames(debugFileName, AXI_MONITOR_FIFO_FULL, &fifoReadBaseAddr, &fifoName, nullptr, 1);
    mPerfMonFifoReadBaseAddress = fifoReadBaseAddr;

    if (fifoCtrlCount != 0 && fifoFullCount != 0)
    {
      mIsTraceHubAvailable = true;
    }

    uint64_t traceFunnelAddr = 0x0;
    getIPCountAddrNames(debugFileName, AXI_TRACE_FUNNEL, &traceFunnelAddr, &fifoName, nullptr, 1);
    mTraceFunnelAddress = traceFunnelAddr;

    // Count accel monitors with stall monitoring turned on
    mStallProfilingNumberSlots = 0;
    for (unsigned int i = 0; i < mAccelProfilingNumberSlots; ++i)
    {
      if ((mAccelmonProperties[i] >> 2) & 0x1)
        mStallProfilingNumberSlots++;
    }

    if (mLogStream.is_open())
    {
      mLogStream << "debug_ip_layout: memory slots = " << mMemoryProfilingNumberSlots << std::endl;
      mLogStream << "debug_ip_layout: accel slots  = " << mAccelProfilingNumberSlots << std::endl;
      mLogStream << "debug_ip_layout: stall slots  = " << mStallProfilingNumberSlots << std::endl;
      mLogStream << "debug_ip_layout: sspm slots   = " << mStreamProfilingNumberSlots << std::endl;

      for (unsigned int i = 0; i < mMemoryProfilingNumberSlots; ++i)
      {
        mLogStream << "debug_ip_layout: AXI_MM_MONITOR slot " << i << ": "
                   << "name = " << mPerfMonSlotName[i]
                   << ", prop = " << static_cast<uint32_t>(mPerfmonProperties[i]) << std::endl;
      }
      for (unsigned int i = 0; i < mAccelProfilingNumberSlots; ++i)
      {
        mLogStream << "debug_ip_layout: ACCEL_MONITOR slot " << i << ": "
                   << "name = " << mAccelMonSlotName[i]
                   << ", prop = " << static_cast<uint32_t>(mAccelmonProperties[i]) << std::endl;
      }
      for (unsigned int i = 0; i < mStreamProfilingNumberSlots; ++i)
      {
        mLogStream << "debug_ip_layout: STREAM_MONITOR slot " << i << ": "
                   << "name = " << mStreamMonSlotName[i]
                   << ", prop = " << static_cast<uint32_t>(mStreamMonProperties[i]) << std::endl;
      }
    }
  }

  // Gets the information about the specified IP from the sysfs debug_ip_table.
  // The IP types are defined in xclbin.h
  uint32_t HwEmShim::getIPCountAddrNames(const std::string debugFileName, int type, uint64_t *baseAddress,
                                         std::string *portNames, uint8_t *properties, size_t size)
  {
    debug_ip_layout *map;
    std::ifstream ifs(debugFileName.c_str(), std::ifstream::binary);

    if (mLogStream.is_open())
      mLogStream << __func__ << ": reading " << debugFileName << " (is_open = " << ifs.is_open() << ")" << std::endl;

    uint32_t count = 0;

    // NOTE: host is always index 0
    if (type == AXI_MM_MONITOR)
    {
      properties[0] = xdp::IP::AIM::mask::PROPERTY_HOST;
      portNames[0] = "host/host";
      ++count;
    }

    char buffer[65536];
    if (ifs)
    {
      //debug_ip_layout max size is 65536
      ifs.read(buffer, 65536);
      if (ifs.gcount() > 0)
      {
        map = (debug_ip_layout *)(buffer);
        for (unsigned int i = 0; i < map->m_count; i++)
        {
          if (count >= size)
            break;
          if (map->m_debug_ip_data[i].m_type == type)
          {
            if (baseAddress)
              baseAddress[count] = map->m_debug_ip_data[i].m_base_address;
            if (portNames)
              portNames[count].assign(map->m_debug_ip_data[i].m_name, 128);
            if (properties)
              properties[count] = map->m_debug_ip_data[i].m_properties;
            ++count;
          }
        }
      }
      ifs.close();
    }
    return count;
  }

  //To get and print the debug messages
  void HwEmShim::fetchAndPrintMessages()
  {

    std::string logMsgs("");
    std::string warning_msgs("");
    std::string stopMsgs("");
    std::string displayMsgs("");
    bool ack = false;
    bool force = false;
    //Read Fifo for size of Info Messages available

    xclGetDebugMessages_RPC_CALL(xclGetDebugMessages, ack, force, displayMsgs, logMsgs, stopMsgs);

    if (mDebugLogStream.is_open() && displayMsgs.empty() == false)
    {
      mDebugLogStream << displayMsgs;
      mDebugLogStream.flush();
    }

    if (mDebugLogStream.is_open() && logMsgs.empty() == false)
    {
      mDebugLogStream << logMsgs;
      mDebugLogStream.flush();
    }

    if (mDebugLogStream.is_open() && warning_msgs.empty() == false)
    {
      mDebugLogStream << warning_msgs;
      mDebugLogStream.flush();
    }

    if (mDebugLogStream.is_open() && stopMsgs.empty() == false)
    {
      mDebugLogStream << stopMsgs;
      mDebugLogStream.flush();
    }
    if (displayMsgs.empty() == false)
    {
      std::cout << displayMsgs;
      std::cout.flush();
    }

    if (logMsgs.empty() == false)
    {
      std::cout << logMsgs;
      std::cout.flush();
    }

    if (warning_msgs.empty() == false)
    {
      std::cout << warning_msgs;
      std::cout.flush();
    }

    if (stopMsgs.empty() == false)
    {
      std::cout << stopMsgs;
      std::cout.flush();
    }
  }

  /*
   * messagesThread(): This thread Prints several messages on to console with a conditional 
   * sleep time.
   * Task 1:
   *  Calls parseLog() with a sleep time of 10,20,30...etc if elapsed time is falls < 300 seconds
   * Task 2: 
   *  Otherwise parseSimulateLog,fetchAndPrintMessages called continuously.
   */
  void HwEmShim::messagesThread()
  {
    using namespace std::chrono_literals;
    if (xclemulation::config::getInstance()->isSystemDPAEnabled() == false)
    {
      return;
    }
    auto l_time = std::chrono::high_resolution_clock::now();
    auto start_time = std::chrono::high_resolution_clock::now();

    unsigned int parseCount = 0;

    xclemulation::sParseLog lParseLog(std::string(getSimPath() + "/simulate.log"), xclemulation::eEmulationType::eHw_Emu, kSimProcessStatus);
    while (get_simulator_started())
    {
      sleep(10);
      if (not get_simulator_started())
       break;
      // If socket is not live then what's the point of having fetchAndPrintMessages? Let's return!
      if (sock->server_started == false) {
        std::cout<<"\n messageThread is exiting now\n";
        return;
      }
      auto l_time_end = std::chrono::high_resolution_clock::now();
      // My wait time > 300 seconds, Let's fetch the exact details behind late connection with Simulator.
      if (std::chrono::duration_cast<std::chrono::seconds>(l_time_end - l_time).count() > kMaxTimeToConnectSimulator) {
       
        std::lock_guard guard{mPrintMessagesLock};
        if (get_simulator_started() == false)
          return;
        // Possibility of deadlock detection?
        parseSimulateLog();
        fetchAndPrintMessages();
      }

      auto end_time = std::chrono::high_resolution_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count() <= kMaxTimeToConnectSimulator) {
        
        if (get_simulator_started() == false) 
          return;
        { // This is for limiting the scope of lock
          std::lock_guard<std::mutex> guard(mPrintMessagesLock);
          dumpDeadlockMessages();
          // Any status message found in parse log file?
          lParseLog.parseLog();
        }
        parseCount++;
        if (parseCount%5 == 0) {
          std::this_thread::sleep_for(5s);
        }
      }
    } //while end.
  }
} // namespace xclhwemhal2
