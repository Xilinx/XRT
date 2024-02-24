/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_CORE_SOURCE

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/common/message.h"
#include "core/common/system.h"
#include "core/include/xdp/fifo.h"

#include "xdp/profile/device/aieTraceS2MM.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#ifndef _WIN32
#ifndef SKIP_IOCTL
// open+ioctl based Profile IP
#include "ioctl_monitors/ioctl_add.h"
#include "ioctl_monitors/ioctl_aieTraceS2MM.h"
#include "ioctl_monitors/ioctl_aim.h"
#include "ioctl_monitors/ioctl_am.h"
#include "ioctl_monitors/ioctl_asm.h"
#include "ioctl_monitors/ioctl_traceFifoFull.h"
#include "ioctl_monitors/ioctl_traceFifoLite.h"
#include "ioctl_monitors/ioctl_traceFunnel.h"
#include "ioctl_monitors/ioctl_traceS2MM.h"
#endif

// open+mmap based Profile IP
#include "mmapped_monitors/mmapped_add.h"
#include "mmapped_monitors/mmapped_aieTraceS2MM.h"
#include "mmapped_monitors/mmapped_aim.h"
#include "mmapped_monitors/mmapped_am.h"
#include "mmapped_monitors/mmapped_asm.h"
#include "mmapped_monitors/mmapped_traceFifoFull.h"
#include "mmapped_monitors/mmapped_traceFifoLite.h"
#include "mmapped_monitors/mmapped_traceFunnel.h"
#include "mmapped_monitors/mmapped_traceS2MM.h"

#endif

#ifndef _WINDOWS
// TODO: Windows build support
// unistd.h is linux only header file
// it is included for read, write, close, lseek64
#include <unistd.h>
#endif

#ifdef _WINDOWS
#define __func__ __FUNCTION__
#endif

#ifdef _WIN32
#pragma warning(disable : 4996 4267)
/* 4996 : Disable warning for use of strncpy */
/* 4267 : Disable warning for conversion of size_t to uint32_t in return
 * statements in "getNumMonitors" */
#endif

namespace xdp {

// Helper functions

// Get the user-specified trace buffer size by parsing
// settings from xrt.ini
uint64_t GetTS2MMBufSize(bool isAIETrace) {
  std::string size_str;

  if (isAIETrace) {
    size_str = xrt_core::config::get_aie_trace_settings_buffer_size();
  } else {
    size_str = xrt_core::config::get_trace_buffer_size();
  }

  std::smatch pieces_match;

  // Default is 1M
  uint64_t bytes = TS2MM_DEF_BUF_SIZE;
  // Regex can parse values like : "1024M" "1G" "8192k"
  const std::regex size_regex("\\s*([0-9]+)\\s*(K|k|M|m|G|g|)\\s*");

  if (std::regex_match(size_str, pieces_match, size_regex)) {
    try {
      if (pieces_match[2] == "K" || pieces_match[2] == "k") {
        bytes = std::stoull(pieces_match[1]) * uint_constants::one_kb;
      } else if (pieces_match[2] == "M" || pieces_match[2] == "m") {
        bytes = std::stoull(pieces_match[1]) * uint_constants::one_mb;
      } else if (pieces_match[2] == "G" || pieces_match[2] == "g") {
        bytes = std::stoull(pieces_match[1]) * uint_constants::one_gb;
      } else {
        bytes = std::stoull(pieces_match[1]);
      }
    } catch (const std::exception &) {
      // User specified number cannot be parsed
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              TS2MM_WARN_MSG_BUFSIZE_DEF);
    }
  } else {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            TS2MM_WARN_MSG_BUFSIZE_DEF);
  }

  // When running on Edge, check if the requested size is greater than 80%
  // of the total possible memory available to the PS.  If so, issue
  // a warning and restrict the size.
  if (isEdge()) {
    uint64_t deviceMemorySize = getPSMemorySize();

    if (deviceMemorySize == 0)
      return bytes;

    double percentSize =
        (100.0 * bytes) / static_cast<double>(deviceMemorySize);

    if (percentSize >= 80.0) {
      // Limit bytes to 80% of the total physical memory
      bytes = static_cast<uint64_t>(std::ceil(0.8 * deviceMemorySize));

      std::stringstream percentSizeStr;
      percentSizeStr << std::fixed << std::setprecision(3) << percentSize;

      std::stringstream bufSizeStr;
      bufSizeStr << std::fixed << std::setprecision(3)
                 << (static_cast<double>(bytes) / (1024.0 * 1024.0)); // In MB

      std::string msg =
          "Requested PL trace buffer size is " + percentSizeStr.str() +
          "% of device memory.  You may run into errors depending upon memory"
          " usage of your application.\nLimiting to " +
          bufSizeStr.str() + "MB.";

      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              msg);
    }

    return bytes;
  }

  // On x86 and Alveo, check against the current hard-coded lmits
  if (bytes > TS2MM_MAX_BUF_SIZE) {
    bytes = TS2MM_MAX_BUF_SIZE;
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            TS2MM_WARN_MSG_BUFSIZE_BIG);
  }

  if (bytes < TS2MM_MIN_BUF_SIZE) {
    bytes = TS2MM_MIN_BUF_SIZE;
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            TS2MM_WARN_MSG_BUFSIZE_SMALL);
  }

  return bytes;
}

// Destructor
DeviceIntf::~DeviceIntf() {
  for (auto mon : mAimList) {
    delete mon;
  }

  for (auto mon : mAmList) {
    delete mon;
  }

  for (auto mon : mAsmList) {
    delete mon;
  }

  for (auto mon : mTraceFunnelList) {
    delete mon;
  }

  for (auto mon : mPlTraceDmaList) {
    delete mon;
  }

  for (auto aieTraceDma : mAieTraceDmaList) {
    delete aieTraceDma;
  }

  for (auto noc : nocList) {
    delete noc;
  }

  mAimList.clear();
  mAmList.clear();
  mAsmList.clear();
  mTraceFunnelList.clear();
  mPlTraceDmaList.clear();
  mAieTraceDmaList.clear();
  nocList.clear();

  delete mFifoCtrl;
  delete mFifoRead;
  delete mDeadlockDetector;

  delete mDevice;
}

void DeviceIntf::setDevice(xdp::Device *devHandle) {
  if (mDevice && mDevice != devHandle) {
    // ERROR : trying to set device when it is already populated with some other
    // device
    return;
  }

  mDevice = devHandle;

  // Once the device is connected, update the bandwidth numbers
  setHostMaxBwRead();
  setHostMaxBwWrite();
  setKernelMaxBwRead();
  setKernelMaxBwWrite();
}

// ***************************************************************************
// Debug IP Layout
// ***************************************************************************

uint32_t DeviceIntf::getNumMonitors(xdp::MonitorType type) {
  if (type == xdp::MonitorType::memory)
    return mAimList.size();

  if (type == xdp::MonitorType::accel)
    return mAmList.size();

  if (type == xdp::MonitorType::str)
    return mAsmList.size();

  if (type == xdp::MonitorType::noc)
    return nocList.size();

  if (type == xdp::MonitorType::stall) {
    uint32_t count = 0;

    for (auto mon : mAmList) {
      if (mon->hasStall())
        count++;
    }

    return count;
  }

  if (type == xdp::MonitorType::host) {
    uint32_t count = 0;

    for (auto mon : mAimList) {
      if (mon->isHostMonitor())
        count++;
    }

    return count;
  }

  // FIFO ?

  if (type == xdp::MonitorType::shell) {
    uint32_t count = 0;

    for (auto mon : mAimList) {
      if (mon->isShellMonitor())
        count++;
    }

    return count;
  }

  // Default
  return 0;
}

std::string DeviceIntf::getMonitorName(xdp::MonitorType type, uint32_t index) {
  if ((type == xdp::MonitorType::memory) && (index < mAimList.size())) {
    return mAimList[index]->getName();
  }

  if ((type == xdp::MonitorType::accel) && (index < mAmList.size())) {
    return mAmList[index]->getName();
  }

  if ((type == xdp::MonitorType::str) && (index < mAsmList.size())) {
    return mAsmList[index]->getName();
  }

  if ((type == xdp::MonitorType::noc) && (index < nocList.size())) {
    return nocList[index]->getName();
  }

  return {};
}

// Same as defined in vpl tcl
// NOTE: This converts the property on the FIFO IP in debug_ip_layout to the
// corresponding FIFO depth.
uint64_t DeviceIntf::getFifoSize() {
  if (mFifoRead)
    return xdp::IP::FIFO::properties::size.at(mFifoRead->getProperties());

  return 0;
}

// ***************************************************************************
// Counters
// ***************************************************************************

// Start device counters performance monitoring
size_t DeviceIntf::startCounters() {
  if (mVerbose) {
    std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
              << ", Start device counters..." << std::endl;
  }

  // Update addresses for debug/profile IP
  // readDebugIPlayout();

  if (!mIsDeviceProfiling)
    return 0;

  size_t size = 0;

  // Axi Interface Mons
  for (auto mon : mAimList) {
    size += mon->startCounter();
  }

  // Accelerator Mons
  for (auto mon : mAmList) {
    size += mon->startCounter();
  }

  // Axi Stream Mons
  for (auto mon : mAsmList) {
    size += mon->startCounter();
  }

  return size;
}

// Stop both profile and trace performance monitoring
size_t DeviceIntf::stopCounters() {
  if (mVerbose) {
    std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
              << ", Stop and reset device counters..." << std::endl;
  }

  if (!mIsDeviceProfiling)
    return 0;

  size_t size = 0;

  // Axi Interface Mons
  for (auto mon : mAimList) {
    size += mon->stopCounter();
  }

#if 0

    // These aren't enabled in IP
    // Accelerator Mons
    for (auto mon : mAmList) {
      size += mon->stopCounter();
    }

    // Axi Stream Mons
    for (auto mon : mAsmList) {
      size += mon->stopCounter();
    }

#endif
  return size;
}

// Read AIM performance counters
size_t DeviceIntf::readCounters(xdp::CounterResults &counterResults) {
  if (mVerbose) {
    std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
              << &counterResults << ", Read device counters..." << std::endl;
  }

  // Initialize all values in struct to 0
  memset(&counterResults, 0, sizeof(xdp::CounterResults));

  if (!mIsDeviceProfiling)
    return 0;

  size_t size = 0;

  // Read all Axi Interface Mons
  for (auto mon : mAimList) {
    size += mon->readCounter(counterResults);
  }

  // Read all Accelerator Mons
  for (auto mon : mAmList) {
    size += mon->readCounter(counterResults);
  }

  // Read all Axi Stream Mons
  for (auto mon : mAsmList) {
    size += mon->readCounter(counterResults);
  }

  return size;
}

// ***************************************************************************
// Timeline Trace
// ***************************************************************************

// Start trace performance monitoring
size_t DeviceIntf::startTrace(uint32_t startTrigger) {
  // StartTrigger Bits:
  // Bit 0: Trace Coarse/Fine     Bit 1: Transfer Trace Ctrl
  // Bit 2: CU Trace Ctrl         Bit 3: INT Trace Ctrl
  // Bit 4: Str Trace Ctrl        Bit 5: Ext Trace Ctrl
  if (mVerbose) {
    std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
              << startTrigger << ", Start device tracing..." << std::endl;
  }

  size_t size = 0;

  // These should be reset before anything
  if (mFifoCtrl)
    mFifoCtrl->reset();

  for (auto mon : mTraceFunnelList) {
    mon->reset();
  }

  // This just writes to trace control register
  // Axi Interface Mons
  for (auto mon : mAimList) {
    size += mon->triggerTrace(startTrigger);
  }

  // Accelerator Mons
  for (auto mon : mAmList) {
    size += mon->triggerTrace(startTrigger);
  }

  // Axi Stream Mons
  for (auto mon : mAsmList) {
    size += mon->triggerTrace(startTrigger);
  }

  uint32_t traceVersion = 0;

  if (!mTraceFunnelList.empty()) {
    if (mTraceFunnelList[0]->compareVersion(1, 0) == -1)
      traceVersion = 1;
  }

  if (mFifoRead)
    mFifoRead->setTraceFormat(traceVersion);

  for (auto mon : mPlTraceDmaList) {
    mon->setTraceFormat(traceVersion);
  }

  // TODO: is this correct?
  for (auto aieTraceDma : mAieTraceDmaList) {
    aieTraceDma->setTraceFormat(traceVersion);
  }

  return size;
}

void DeviceIntf::clockTraining(bool force) {
  if (mTraceFunnelList.empty())
    return;

  // Trace Funnel > 1.0 supports continuous training
  if (mTraceFunnelList[0]->compareVersion(1, 0) == -1 || force == true) {
    for (auto mon : mTraceFunnelList) {
      mon->initiateClockTraining();
    }
  }
}

// Stop trace performance monitoring
size_t DeviceIntf::stopTrace() {
  if (mVerbose) {
    std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
              << ", Stop and reset device tracing..." << std::endl;
  }

  if (!mIsDeviceProfiling || !mFifoCtrl)
    return 0;

  return mFifoCtrl->reset();
}

// Get trace word count
uint32_t DeviceIntf::getTraceCount() {
  if (mVerbose) {
    std::cout << __func__ << ", " << std::this_thread::get_id() << std::endl;
  }

  if (!mIsDeviceProfiling || !mFifoCtrl)
    return 0;

  return mFifoCtrl->getNumTraceSamples();
}

// Read all values from APM trace AXI stream FIFOs
size_t DeviceIntf::readTrace(uint32_t *&traceData) {
  if (!mIsDeviceProfiling || !mFifoRead)
    return 0;

  size_t size = 0;
  size += mFifoRead->readTrace(traceData, getTraceCount());

  return size;
}

void DeviceIntf::readDebugIPlayout() {
  if (mIsDebugIPlayoutRead || !mDevice)
    return;

#ifndef _WIN32
  std::string path = mDevice->getDebugIPlayoutPath();

  if (path.empty()) {
    // error ? : for HW_emu this will be empty for now ; but as of current
    // status should not have been called
    return;
  }

  uint32_t liveProcessesOnDevice = mDevice->getNumLiveProcesses();

  if (liveProcessesOnDevice > 1) {
    /* More than 1 process on device. Device Profiling for multi-process not
     * supported yet.
     */
    std::string warnMsg =
        "Multiple live processes running on device. Hardware Debug and "
        "Profiling data will be unavailable for this process.";
    std::cout << warnMsg << std::endl;
    // xrt_core::message::send(xrt_core::message::severity_level::warning,
    // "XRT", warnMsg) ;
    mIsDeviceProfiling = false;
    mIsDebugIPlayoutRead = true;
    return;
  }

  std::ifstream ifs(path.c_str(), std::ifstream::binary);

  if (!ifs) {
    return;
  }

  char buffer[65536];
  // debug_ip_layout max size is 65536
  ifs.read(buffer, 65536);

  debug_ip_layout *map;

  if (ifs.gcount() > 0) {
    map = (debug_ip_layout *)(buffer);
#else
  //The vector<char> buffer is a chunk of raw bytes, not a string
  std::vector<char> buffer;
  buffer = mDevice->getDebugIpLayout();
  auto map = reinterpret_cast<debug_ip_layout *>(buffer.data());
#endif

    xrt_core::system::monitor_access_type accessType =
        xrt_core::get_monitor_access_type();

    /* Currently, only PCIeLinux Device flow uses open+ioctl and hence
     * specialized monitors are instantiated. All other flows(including PCIe
     * Windows) use the older mechanism and should use old monitor abstraction.
     * Also, user space cannot access profiling subdvices while running inside
     * containers, so use xclRead/Write based flow.
     */
    if (xrt_core::system::monitor_access_type::bar == accessType ||
        true == xrt_core::config::get_container()) {
      for (uint64_t i = 0; i < map->m_count; i++) {
        switch (map->m_debug_ip_data[i].m_type) {
        case AXI_MM_MONITOR:
          mAimList.push_back(new AIM(mDevice, i, &(map->m_debug_ip_data[i])));
          break;

        case ACCEL_MONITOR:
          mAmList.push_back(new AM(mDevice, i, &(map->m_debug_ip_data[i])));
          break;

        case AXI_STREAM_MONITOR:
          mAsmList.push_back(new ASM(mDevice, i, &(map->m_debug_ip_data[i])));
          break;

        case AXI_MONITOR_FIFO_LITE:
          mFifoCtrl = new TraceFifoLite(mDevice, i, &(map->m_debug_ip_data[i]));
          break;

        case AXI_MONITOR_FIFO_FULL:
          mFifoRead = new TraceFifoFull(mDevice, i, &(map->m_debug_ip_data[i]));
          break;

        case AXI_TRACE_FUNNEL:
          mTraceFunnelList.push_back(
              new TraceFunnel(mDevice, i, &(map->m_debug_ip_data[i])));
          break;

        case TRACE_S2MM:

          // AIE trace potentially uses multiple data movers (based on BW
          // requirements)
          if (map->m_debug_ip_data[i].m_properties & TS2MM_AIE_TRACE_MASK)
            mAieTraceDmaList.push_back(
                new AIETraceS2MM(mDevice, i, &(map->m_debug_ip_data[i])));
          else
            mPlTraceDmaList.push_back(
                new TraceS2MM(mDevice, i, &(map->m_debug_ip_data[i])));

          break;

        case AXI_NOC:
          nocList.push_back(new NOC(mDevice, i, &(map->m_debug_ip_data[i])));
          break;

        case ACCEL_DEADLOCK_DETECTOR:
          mDeadlockDetector =
              new DeadlockDetector(mDevice, i, &(map->m_debug_ip_data[i]));
          break;

        case HSDP_TRACE: {
          // 2nd and 1st LSB (not the 0th) indicate AIE only (00) or PL only
          // (01) or AIE+PL (11)
          uint8_t bitsForPL =
              (((map->m_debug_ip_data[i]).m_properties) >> 1) & 3;

          if (1 == bitsForPL || 3 == bitsForPL) {
            mHSDPforPL = true;
          }

          break;
        }

        case AXI_STREAM_PROTOCOL_CHECKER:
        default:
          break;
        }
      }
    }

#ifndef _WIN32
    else if (xrt_core::system::monitor_access_type::mmap == accessType) {
      for (uint64_t i = 0; i < map->m_count; i++) {
        switch (map->m_debug_ip_data[i].m_type) {
        case AXI_MM_MONITOR: {
          MMappedAIM *pMon = new MMappedAIM(mDevice, i, mAimList.size(),
                                            &(map->m_debug_ip_data[i]));

          if (pMon->isMMapped()) {
            mAimList.push_back(pMon);
          } else {
            delete pMon;
            pMon = nullptr;
          }

          break;
        }

        case ACCEL_MONITOR: {
          MMappedAM *pMon = new MMappedAM(mDevice, i, mAmList.size(),
                                          &(map->m_debug_ip_data[i]));

          if (pMon->isMMapped()) {
            mAmList.push_back(pMon);
          } else {
            delete pMon;
            pMon = nullptr;
          }

          break;
        }

        case AXI_STREAM_MONITOR: {
          MMappedASM *pMon = new MMappedASM(mDevice, i, mAsmList.size(),
                                            &(map->m_debug_ip_data[i]));

          if (pMon->isMMapped()) {
            mAsmList.push_back(pMon);
          } else {
            delete pMon;
            pMon = nullptr;
          }

          break;
        }

        case AXI_MONITOR_FIFO_LITE: {
          mFifoCtrl =
              new MMappedTraceFifoLite(mDevice, i, &(map->m_debug_ip_data[i]));

          if (!mFifoCtrl->isMMapped()) {
            delete mFifoCtrl;
            mFifoCtrl = nullptr;
          }

          break;
        }

        case AXI_MONITOR_FIFO_FULL: {
          mFifoRead =
              new MMappedTraceFifoFull(mDevice, i, &(map->m_debug_ip_data[i]));

          if (!mFifoRead->isMMapped()) {
            delete mFifoRead;
            mFifoRead = nullptr;
          }

          break;
        }

        case AXI_TRACE_FUNNEL: {
          MMappedTraceFunnel *pMon = new MMappedTraceFunnel(
              mDevice, i, mTraceFunnelList.size(), &(map->m_debug_ip_data[i]));

          if (pMon->isMMapped()) {
            mTraceFunnelList.push_back(pMon);
          } else {
            delete pMon;
            pMon = nullptr;
          }

          break;
        }

        case TRACE_S2MM: {
          // AIE trace potentially uses multiple data movers (based on BW
          // requirements)
          if (map->m_debug_ip_data[i].m_properties & TS2MM_AIE_TRACE_MASK) {
            TraceS2MM *aieTraceDma =
                new MMappedAIETraceS2MM(mDevice, i, mAieTraceDmaList.size(),
                                        &(map->m_debug_ip_data[i]));

            if (aieTraceDma->isMMapped()) {
              mAieTraceDmaList.push_back(aieTraceDma);
            } else {
              delete aieTraceDma;
            }
          } else {
            TraceS2MM *plTraceDma = new MMappedTraceS2MM(
                mDevice, i, mPlTraceDmaList.size(), &(map->m_debug_ip_data[i]));

            if (plTraceDma->isMMapped()) {
              mPlTraceDmaList.push_back(plTraceDma);
            } else {
              delete plTraceDma;
            }
          }

          break;
        }

        // case AXI_NOC :
        //{
        // MMappedNOC* pNoc = new MMappedNOC(mDevice, i, nocList.size(),
        // &(map->m_debug_ip_data[i])); if(pNoc->isMMapped()) {
        // nocList.push_back(pNoc);
        // } else {
        // delete pNoc;
        // pNoc = nullptr;
        // }
        // break;
        //}
        case ACCEL_DEADLOCK_DETECTOR: {
          mDeadlockDetector = new MMappedDeadlockDetector(
              mDevice, i, &(map->m_debug_ip_data[i]));

          if (!mDeadlockDetector->isMMapped()) {
            delete mDeadlockDetector;
            mDeadlockDetector = nullptr;
          }

          break;
        }

        case HSDP_TRACE: {
          // 2nd and 1st LSB (not the 0th) indicate AIE only (00) or PL only
          // (01) or AIE+PL (11)
          uint8_t bitsForPL =
              (((map->m_debug_ip_data[i]).m_properties) >> 1) & 3;

          if (1 == bitsForPL || 3 == bitsForPL) {
            mHSDPforPL = true;
          }

          break;
        }

        default:
          break;
        }
      }
    }

#ifndef SKIP_IOCTL
    else if (xrt_core::system::monitor_access_type::ioctl == accessType) {
      for (uint64_t i = 0; i < map->m_count; i++) {
        switch (map->m_debug_ip_data[i].m_type) {
        case AXI_MM_MONITOR: {
          IOCtlAIM *pMon = new IOCtlAIM(mDevice, i, mAimList.size(),
                                        &(map->m_debug_ip_data[i]));

          if (pMon->isOpened()) {
            mAimList.push_back(pMon);
          } else {
            delete pMon;
            pMon = nullptr;
          }

          break;
        }

        case ACCEL_MONITOR: {
          IOCtlAM *pMon = new IOCtlAM(mDevice, i, mAmList.size(),
                                      &(map->m_debug_ip_data[i]));

          if (pMon->isOpened()) {
            mAmList.push_back(pMon);
          } else {
            delete pMon;
            pMon = nullptr;
          }

          break;
        }

        case AXI_STREAM_MONITOR: {
          IOCtlASM *pMon = new IOCtlASM(mDevice, i, mAsmList.size(),
                                        &(map->m_debug_ip_data[i]));

          if (pMon->isOpened()) {
            mAsmList.push_back(pMon);
          } else {
            delete pMon;
            pMon = nullptr;
          }

          break;
        }

        case AXI_MONITOR_FIFO_LITE: {
          mFifoCtrl =
              new IOCtlTraceFifoLite(mDevice, i, &(map->m_debug_ip_data[i]));

          if (!mFifoCtrl->isOpened()) {
            delete mFifoCtrl;
            mFifoCtrl = nullptr;
          }

          break;
        }

        case AXI_MONITOR_FIFO_FULL: {
          mFifoRead =
              new IOCtlTraceFifoFull(mDevice, i, &(map->m_debug_ip_data[i]));

          if (!mFifoRead->isOpened()) {
            delete mFifoRead;
            mFifoRead = nullptr;
          }

          break;
        }

        case AXI_TRACE_FUNNEL: {
          IOCtlTraceFunnel *pMon = new IOCtlTraceFunnel(
              mDevice, i, mTraceFunnelList.size(), &(map->m_debug_ip_data[i]));

          if (pMon->isOpened()) {
            mTraceFunnelList.push_back(pMon);
          } else {
            delete pMon;
            pMon = nullptr;
          }

          break;
        }

        case TRACE_S2MM: {
          // AIE trace potentially uses multiple data movers (based on BW
          // requirements)
          if (map->m_debug_ip_data[i].m_properties & TS2MM_AIE_TRACE_MASK) {
            TraceS2MM *aieTraceDma =
                new IOCtlAIETraceS2MM(mDevice, i, mAieTraceDmaList.size(),
                                      &(map->m_debug_ip_data[i]));

            if (aieTraceDma->isOpened()) {
              mAieTraceDmaList.push_back(aieTraceDma);
            } else {
              delete aieTraceDma;
            }
          } else {
            TraceS2MM *plTraceDma = new IOCtlTraceS2MM(
                mDevice, i, mPlTraceDmaList.size(), &(map->m_debug_ip_data[i]));

            if (plTraceDma->isOpened()) {
              mPlTraceDmaList.push_back(plTraceDma);
            } else {
              delete plTraceDma;
            }
          }

          break;
        }

        case ACCEL_DEADLOCK_DETECTOR: {
          mDeadlockDetector =
              new IOCtlDeadlockDetector(mDevice, i, &(map->m_debug_ip_data[i]));

          if (!mDeadlockDetector->isOpened()) {
            delete mDeadlockDetector;
            mDeadlockDetector = nullptr;
          }

          break;
        }

        case HSDP_TRACE: {
          // 2nd and 1st LSB (not the 0th) indicate AIE only (00) or PL only
          // (01) or AIE+PL (11)
          uint8_t bitsForPL =
              (((map->m_debug_ip_data[i]).m_properties) >> 1) & 3;

          if (1 == bitsForPL || 3 == bitsForPL) {
            mHSDPforPL = true;
          }

          break;
        }

        case AXI_STREAM_PROTOCOL_CHECKER:
        case AXI_NOC:
        default:
          break;
        }
      }
    }

#endif
    else {
      // other access types not supported yet
    }
  }

  ifs.close();
#endif

#if 0

    for (auto mon : mAimList)
    {
      mon->showProperties();
    }

    for (auto mon : mAmList)
    {
      mon->showProperties();
    }

    for (auto mon : mAsmList)
    {
      mon->showProperties();
    }

    for (auto mon : mTraceFunnelList)
    {
      mon->showProperties();
    }

    for (auto mon : mPlTraceDmaList)
    {
      mon->showProperties();
    }

    for (auto mon : mAieTraceDmaList)
    {
      mon->showProperties();
    }

    for (auto noc : nocList)
    {
      noc->showProperties();
    }

    if (mFifoCtrl)
      mFifoCtrl->showProperties();

    if (mFifoRead)
      mFifoRead->showProperties();

#endif

  mIsDebugIPlayoutRead = true;
}

void DeviceIntf::configureDataflow(bool *ipConfig) {
  // this ipConfig only tells whether the corresponding CU has ap_control_chain
  // : could have been just a property on the monitor set at compile time (in
  // debug_ip_layout)
  if (!ipConfig)
    return;

  uint32_t i = 0;

  for (auto mon : mAmList) {
    mon->configureDataflow(ipConfig[i++]);
  }
}

void DeviceIntf::configureFa(bool *ipConfig) {
  // this ipConfig only tells whether the corresponding CU uses Fast Adapter
  if (!ipConfig)
    return;

  uint32_t i = 0;

  for (auto mon : mAmList) {
    mon->configureFa(ipConfig[i++]);
  }
}

void DeviceIntf::configAmContext(const std::string &ctx_info) {
  if (ctx_info.empty())
    return;

  for (auto mon : mAmList) {
    mon->disable();
  }
}

size_t DeviceIntf::allocTraceBuf(uint64_t sz, uint8_t memIdx) {
  std::lock_guard<std::mutex> lock(traceLock);
  auto bufId = mDevice->alloc(sz, memIdx);

  if (bufId) {
    // Can't read a buffer xrt hasn't written to
    mDevice->sync(bufId, sz, 0, xdp::Device::direction::HOST2DEVICE);
  }

  return bufId;
}

void DeviceIntf::freeTraceBuf(size_t id) {
  std::lock_guard<std::mutex> lock(traceLock);
  mDevice->free(id);
}

/**
 * Takes the offset inside the mapped buffer
 * and syncs it with device and returns its virtual address.
 * We can read the entire buffer in one go if we want to
 * or choose to read in chunks
 */
void *DeviceIntf::syncTraceBuf(size_t id, uint64_t offset, uint64_t bytes) {
  std::lock_guard<std::mutex> lock(traceLock);
  auto addr = mDevice->map(id);

  if (!addr)
    return nullptr;

  mDevice->sync(id, bytes, offset, xdp::Device::direction::DEVICE2HOST);
  mDevice->unmap(id);
  return static_cast<char *>(addr) + offset;
}

xclBufferExportHandle DeviceIntf::exportTraceBuf(size_t id) {
  std::lock_guard<std::mutex> lock(traceLock);
  return mDevice->exportBuffer(id);
}

uint64_t DeviceIntf::getTraceBufDeviceAddr(size_t id) {
  return mDevice->getBufferDeviceAddr(id);
}

// All buffers have to be 4k Aligned
uint64_t DeviceIntf::getAlignedTraceBufSize(uint64_t total_bytes,
                                            unsigned int num_chunks) {
  constexpr uint64_t TRACE_BUFFER_4K_MASK = 0xfffffffffffff000;

  if (!num_chunks)
    return 0;

  uint64_t aligned_size = (total_bytes / num_chunks) & TRACE_BUFFER_4K_MASK;

  if (aligned_size < TS2MM_MIN_BUF_SIZE)
    aligned_size = TS2MM_MIN_BUF_SIZE;

  if (xrt_core::config::get_verbosity() >=
      static_cast<unsigned int>(xrt_core::message::severity_level::info)) {
    std::stringstream info_msg;
    info_msg << "Setting 4K aligned trace buffer size to : " << aligned_size
             << " for num chunks : " << num_chunks;
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
                            info_msg.str());
  }

  return aligned_size;
}

// Reset PL trace data movers
void DeviceIntf::resetTS2MM(uint64_t index) {
  if (index >= mPlTraceDmaList.size())
    return;

  mPlTraceDmaList[index]->reset();
}

// Initialize PL trace data mover
void DeviceIntf::initTS2MM(uint64_t index, uint64_t bufSz, uint64_t bufAddr,
                           bool circular) {
  if (index >= mPlTraceDmaList.size())
    return;

  mPlTraceDmaList[index]->init(bufSz, bufAddr, circular);
}

// Get word count written by PL trace data mover
uint64_t DeviceIntf::getWordCountTs2mm(uint64_t index, bool final) {
  if (index >= mPlTraceDmaList.size())
    return 0;

  return mPlTraceDmaList[index]->getWordCount(final);
}

// Get memory index of trace data mover
uint8_t DeviceIntf::getTS2MmMemIndex(uint64_t index) {
  if (index >= mPlTraceDmaList.size())
    return 0;

  return mPlTraceDmaList[index]->getMemIndex();
}

// Parse trace buffer data after reading from FIFO or DDR
void DeviceIntf::parseTraceData(uint64_t index, void *traceData, uint64_t bytes,
                                std::vector<xdp::TraceEvent> &traceVector) {
  if (index >= mPlTraceDmaList.size())
    return;

  mPlTraceDmaList[index]->parseTraceBuf(traceData, bytes, traceVector);
}

// Reset AIE trace data movers
void DeviceIntf::resetAIETs2mm(uint64_t index) {
  if (index >= mAieTraceDmaList.size())
    return;

  mAieTraceDmaList[index]->reset();
}

// Initialize an AIE trace data mover
void DeviceIntf::initAIETs2mm(uint64_t bufSz, uint64_t bufAddr, uint64_t index,
                              bool circular) {
  if (index >= mAieTraceDmaList.size())
    return;

  mAieTraceDmaList[index]->init(bufSz, bufAddr, circular);
}

// Get word count written by AIE trace data mover
uint64_t DeviceIntf::getWordCountAIETs2mm(uint64_t index, bool final) {
  if (index >= mAieTraceDmaList.size())
    return 0;

  return mAieTraceDmaList[index]->getWordCount(final);
}

// Get memory index of AIE trace data mover
uint8_t DeviceIntf::getAIETs2mmMemIndex(uint64_t index) {
  if (index >= mAieTraceDmaList.size())
    return 0;

  return mAieTraceDmaList[index]->getMemIndex();
}

void DeviceIntf::setHostMaxBwRead() {
  mHostMaxReadBW = mDevice->getHostMaxBwRead();
}

void DeviceIntf::setHostMaxBwWrite() {
  mHostMaxWriteBW = mDevice->getHostMaxBwWrite();
}

void DeviceIntf::setKernelMaxBwRead() {
  mKernelMaxReadBW = mDevice->getKernelMaxBwRead();
}

void DeviceIntf::setKernelMaxBwWrite() {
  mKernelMaxWriteBW = mDevice->getKernelMaxBwWrite();
}

uint32_t DeviceIntf::getDeadlockStatus() {
  if (mDeadlockDetector)
    return mDeadlockDetector->getDeadlockStatus();

  return 0;
}

} // namespace xdp
