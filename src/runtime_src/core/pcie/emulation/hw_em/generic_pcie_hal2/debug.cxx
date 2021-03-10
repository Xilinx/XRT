/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "shim.h"
#include "xclbin.h"
#include "xcl_perfmon_parameters.h"

#include <iostream>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <thread>
#include <vector>
#include <ctime>
#include <string>
#include <chrono>

#ifndef _WINDOWS
// TODO: Windows build support
//    unistd.h is linux only header file
//    it is included for read, write, close, lseek64
#include <unistd.h>
#endif

#ifdef _WINDOWS
#define __func__ __FUNCTION__
#endif

namespace xclhwemhal2 {

  void HwEmShim::readDebugIpLayout(const std::string debugFileName)
  {
    //
    // Profiling - addresses and names
    // Parsed from debug_ip_layout.rtd contained in xclbin
    if (mLogStream.is_open()) {
      mLogStream << "debug_ip_layout: reading profile addresses and names..." << std::endl;
    }

    memset(mPerfmonProperties, 0, sizeof(uint8_t)*XAIM_MAX_NUMBER_SLOTS);
    memset(mAccelmonProperties, 0, sizeof(uint8_t)*XAM_MAX_NUMBER_SLOTS);
    memset(mStreamMonProperties, 0, sizeof(uint8_t)*XASM_MAX_NUMBER_SLOTS);

    mMemoryProfilingNumberSlots = getIPCountAddrNames(debugFileName, AXI_MM_MONITOR, mPerfMonBaseAddress,
      mPerfMonSlotName, mPerfmonProperties, XAIM_MAX_NUMBER_SLOTS);
    
    mAccelProfilingNumberSlots = getIPCountAddrNames(debugFileName, ACCEL_MONITOR, mAccelMonBaseAddress,
      mAccelMonSlotName, mAccelmonProperties, XAM_MAX_NUMBER_SLOTS);

    mStreamProfilingNumberSlots = getIPCountAddrNames(debugFileName, AXI_STREAM_MONITOR, mStreamMonBaseAddress,
      mStreamMonSlotName, mStreamMonProperties, XASM_MAX_NUMBER_SLOTS);

    mIsDeviceProfiling = (mMemoryProfilingNumberSlots > 0 || mAccelProfilingNumberSlots > 0 || mStreamProfilingNumberSlots > 0);

    std::string fifoName;
    uint64_t fifoCtrlBaseAddr = 0x0;
    uint32_t fifoCtrlCount = getIPCountAddrNames(debugFileName, AXI_MONITOR_FIFO_LITE, &fifoCtrlBaseAddr, &fifoName, nullptr, 1);
    mPerfMonFifoCtrlBaseAddress = fifoCtrlBaseAddr;

    uint64_t fifoReadBaseAddr = 0x0;
    uint32_t fifoFullCount = getIPCountAddrNames(debugFileName, AXI_MONITOR_FIFO_FULL, &fifoReadBaseAddr, &fifoName, nullptr, 1);
    mPerfMonFifoReadBaseAddress = fifoReadBaseAddr;

    if(fifoCtrlCount != 0 && fifoFullCount != 0) {
    	mIsTraceHubAvailable = true;
    }

    uint64_t traceFunnelAddr = 0x0;
    getIPCountAddrNames(debugFileName, AXI_TRACE_FUNNEL, &traceFunnelAddr, &fifoName, nullptr, 1);
    mTraceFunnelAddress = traceFunnelAddr;

    // Count accel monitors with stall monitoring turned on
    mStallProfilingNumberSlots = 0;
    for (unsigned int i = 0; i < mAccelProfilingNumberSlots; ++i) {
      if ((mAccelmonProperties[i] >> 2) & 0x1)
        mStallProfilingNumberSlots++;
    }

    if (mLogStream.is_open()) {
      mLogStream << "debug_ip_layout: memory slots = " << mMemoryProfilingNumberSlots << std::endl;
      mLogStream << "debug_ip_layout: accel slots  = " << mAccelProfilingNumberSlots << std::endl;
      mLogStream << "debug_ip_layout: stall slots  = " << mStallProfilingNumberSlots << std::endl;
      mLogStream << "debug_ip_layout: sspm slots   = " << mStreamProfilingNumberSlots << std::endl;

      for (unsigned int i = 0; i < mMemoryProfilingNumberSlots; ++i) {
        mLogStream << "debug_ip_layout: AXI_MM_MONITOR slot " << i << ": "
                   << "name = " << mPerfMonSlotName[i]
                   << ", prop = " << static_cast <uint32_t>(mPerfmonProperties[i]) << std::endl;
      }
      for (unsigned int i = 0; i < mAccelProfilingNumberSlots; ++i) {
        mLogStream << "debug_ip_layout: ACCEL_MONITOR slot " << i << ": "
                   << "name = " << mAccelMonSlotName[i]
                   << ", prop = " << static_cast <uint32_t>(mAccelmonProperties[i]) << std::endl;
      }
      for (unsigned int i = 0; i < mStreamProfilingNumberSlots; ++i) {
        mLogStream << "debug_ip_layout: STREAM_MONITOR slot " << i << ": "
                   << "name = " << mStreamMonSlotName[i]
                   << ", prop = " << static_cast <uint32_t>(mStreamMonProperties[i]) << std::endl;
      }
    }
  }

  // Gets the information about the specified IP from the sysfs debug_ip_table.
  // The IP types are defined in xclbin.h
  uint32_t HwEmShim::getIPCountAddrNames(const std::string debugFileName, int type, uint64_t *baseAddress,
                                         std::string * portNames, uint8_t *properties, size_t size) {
    debug_ip_layout *map;
    std::ifstream ifs(debugFileName.c_str(), std::ifstream::binary);

    if (mLogStream.is_open())
      mLogStream << __func__ << ": reading " << debugFileName << " (is_open = " << ifs.is_open() << ")" << std::endl;

    uint32_t count = 0;

    // NOTE: host is always index 0
    if (type == AXI_MM_MONITOR) {
      properties[0] = XAIM_HOST_PROPERTY_MASK;
      portNames[0] = "host/host";
      ++count;
    }

    char buffer[65536];
    if (ifs) {
      //debug_ip_layout max size is 65536
      ifs.read(buffer, 65536);
      if (ifs.gcount() > 0) {
        map = (debug_ip_layout*)(buffer);
        for (unsigned int i = 0; i < map->m_count; i++) {
          if (count >= size) break;
          if (map->m_debug_ip_data[i].m_type == type) {
            if (baseAddress) baseAddress[count] = map->m_debug_ip_data[i].m_base_address;
            if (portNames)   portNames[count].assign(map->m_debug_ip_data[i].m_name, 128);
            if (properties)  properties[count]  = map->m_debug_ip_data[i].m_properties;
            ++count;
          }
        }
      }
      ifs.close();
    }
    return count;
  }

  //To get and print the debug messages
  void HwEmShim::fetchAndPrintMessages() {

	  std::string logMsgs("");
	  std::string warning_msgs("");
	  std::string stopMsgs("");
	  std::string displayMsgs("");
	  bool ack =false;
	  bool force =false;
	  //Read Fifo for size of Info Messages available

	  xclGetDebugMessages_RPC_CALL(xclGetDebugMessages,ack,force,displayMsgs,logMsgs,stopMsgs);

	  if(mDebugLogStream.is_open() && displayMsgs.empty() == false) {
		mDebugLogStream << displayMsgs;
		mDebugLogStream.flush();
	  }

	  if(mDebugLogStream.is_open() && logMsgs.empty() == false) {
		mDebugLogStream << logMsgs;
		mDebugLogStream.flush();
	  }

	  if(mDebugLogStream.is_open() && warning_msgs.empty() == false) {
		mDebugLogStream << warning_msgs;
		mDebugLogStream.flush();
	  }

	  if(mDebugLogStream.is_open() && stopMsgs.empty() == false) {
		mDebugLogStream << stopMsgs;
		mDebugLogStream.flush();
	  }
	  if(displayMsgs.empty() == false)
	  {
	 	std::cout<<displayMsgs;
	 	std::cout.flush();
	  }

	  if(logMsgs.empty() == false)
	  {
	    std::cout<<logMsgs;
	    std::cout.flush();
	  }

	  if(warning_msgs.empty() == false)
	  {
	    std::cout<<warning_msgs;
	    std::cout.flush();
	  }

	  if(stopMsgs.empty() == false)
	  {
	    std::cout<<stopMsgs;
	    std::cout.flush();
	  }
  }

  /*
   * messagesThread()
   */
void messagesThread(xclhwemhal2::HwEmShim* inst) {
	if (xclemulation::config::getInstance()->isSystemDPAEnabled() == false) {
		return;
	}
	static auto l_time = std::chrono::high_resolution_clock::now();
	while (inst && inst->get_simulator_started()) {
		sleep(10);
		if (inst->get_simulator_started() == false) {
			return;
		}
		auto l_time_end = std::chrono::high_resolution_clock::now();
		if (std::chrono::duration<double>(l_time_end - l_time).count() > 300) {
			l_time = std::chrono::high_resolution_clock::now();
			inst->mPrintMessagesLock.lock();
			if (inst->get_simulator_started() == false) {
				inst->mPrintMessagesLock.unlock();
				return;
			}
                        inst->parseSimulateLog();
                        inst->fetchAndPrintMessages();
                        inst->mPrintMessagesLock.unlock();
		}
	}
}
} // namespace xclhwemhal2
