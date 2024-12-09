/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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

#define XDP_CORE_SOURCE

#include <memory>
#include <map>

#include "xdp/profile/database/static_info/device_info.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/static_info/xclbin_info.h"
#include "xdp/profile/writer/vp_base/guidance_rules.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/plugin/vp_base/info.h"

#include "core/include/xrt/detail/xclbin.h"
#include "core/common/time.h"

// An anonymous namespace for all of the different guidance rules
namespace {

  static void deviceExecTime(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::opencl_counters)) return ;

    if (xdp::getFlowMode() == xdp::SW_EMU) {
      std::string deviceName =
        db->getStaticInfo().getSoftwareEmulationDeviceName();
      uint64_t execTime = db->getStats().getDeviceActiveTime(deviceName);

      fout << "DEVICE_EXEC_TIME," << deviceName << ","
           << (double)execTime / 1e06 << ",\n" ;
    }
    else {
      auto deviceInfos = db->getStaticInfo().getDeviceInfos();

      for (auto device : deviceInfos) {
        std::string deviceName = device->getUniqueDeviceName();
        uint64_t execTime = db->getStats().getDeviceActiveTime(deviceName);

        fout << "DEVICE_EXEC_TIME," << deviceName << ","
             << (double)execTime / 1e06 << ",\n" ;
      }
    }
  }

  static void cuCalls(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (xdp::getFlowMode() == xdp::SW_EMU) {
      std::map<std::tuple<std::string, std::string, std::string>,
               xdp::TimeStatistics> cuStats =
        db->getStats().getComputeUnitExecutionStats();

      for (const auto& iter : cuStats) {
        fout << "CU_CALLS,"
             << db->getStaticInfo().getSoftwareEmulationDeviceName()
             << "|" << std::get<0>(iter.first) << ","
             << iter.second.numExecutions << ",\n" ;
      }
    }
    else {
      auto deviceInfos = db->getStaticInfo().getDeviceInfos();
      for (auto device : deviceInfos) {
        auto& loadedConfigs = device->getLoadedConfigs();
        for (const auto& config : loadedConfigs) {
          xdp::XclbinInfo* xclbin = config->getPlXclbin();
          if (!xclbin)
            continue;
          for (const auto& cu : xclbin->pl.cus) {
            std::string cuName = cu.second->getName();
            std::vector<std::pair<std::string, xdp::TimeStatistics>> cuCalls =
              db->getStats().getComputeUnitExecutionStats(cuName);
            uint64_t execCount = 0;
            for (const auto& cuCall : cuCalls) {
              execCount += cuCall.second.numExecutions;
            }
            if (execCount != 0) {
              fout << "CU_CALLS," << device->getUniqueDeviceName() << "|"
                   << cu.second->getName() << ","
                   << execCount << ",\n";
            }
          }
        }
      }
    }
  }

  static void numMonitors(xdp::VPDatabase* db, std::ofstream& fout)
  {
    struct MonInfo {
      std::string type ;
      uint64_t numTraceEnabled ;
      uint64_t numTotal ;

      MonInfo(const std::string& s, uint64_t trace, uint64_t total) :
        type(s), numTraceEnabled(trace), numTotal(total)
      {}
      ~MonInfo() {}
    } ;

    std::map<uint8_t, std::unique_ptr<MonInfo>> monitors ;

    monitors[ACCEL_MONITOR]      =
      std::make_unique<MonInfo>("XCL_PERF_MON_ACCEL", 0, 0);
    monitors[AXI_MM_MONITOR]     =
      std::make_unique<MonInfo>("XCL_PERF_MON_MEMORY", 0, 0);
    monitors[AXI_STREAM_MONITOR] =
      std::make_unique<MonInfo>("XCL_PERF_MON_STR", 0, 0);

    auto deviceInfos = db->getStaticInfo().getDeviceInfos();
    for (auto device : deviceInfos) {
      auto& loadedConfigs = device->getLoadedConfigs();
      for (const auto& config : loadedConfigs) {
        xdp::XclbinInfo* xclbin = config->getPlXclbin();
        if (!xclbin)
          continue;
        monitors[ACCEL_MONITOR]->numTotal += xclbin->pl.ams.size();
        for (auto am : xclbin->pl.ams) {
          if (am->traceEnabled)
            monitors[ACCEL_MONITOR]->numTraceEnabled++ ;
        }

        monitors[AXI_MM_MONITOR]->numTotal += xclbin->pl.aims.size();
        for (auto aim : xclbin->pl.aims) {
          if (aim->traceEnabled)
            monitors[AXI_MM_MONITOR]->numTraceEnabled++ ;
        }

        monitors[AXI_STREAM_MONITOR]->numTotal += xclbin->pl.asms.size();
        for (auto mon : xclbin->pl.asms) {
          if (mon->traceEnabled)
            monitors[AXI_STREAM_MONITOR]->numTraceEnabled++ ;
        }
      }
      for (auto& mon : monitors) {
        fout << "NUM_MONITORS,"
             << device->getUniqueDeviceName() << "|"
             << mon.second->type << "|"
             << mon.second->numTraceEnabled << ","
             << mon.second->numTotal << ",\n" ;

        // Reset the numbers
        mon.second->numTotal = 0 ;
        mon.second->numTraceEnabled = 0 ;
      }
    }
  }

  static void migrateMem(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::opencl_trace)) return ;

    uint64_t numCalls = db->getStats().getNumMigrateMemCalls();

    fout << "MIGRATE_MEM,host," << numCalls << ",\n" ;
  }

  static void memoryUsage(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (xdp::getFlowMode() == xdp::SW_EMU) {
      std::map<std::string, bool> memUsage =
        db->getStaticInfo().getSoftwareEmulationMemUsage();
      for (const auto& iter : memUsage) {
        fout << "MEMORY_USAGE," << iter.first << "," << iter.second << ",\n";
      }
    }
    else {
      auto deviceInfos = db->getStaticInfo().getDeviceInfos() ;

      for (auto device : deviceInfos) {
        auto& loadedConfigs = device->getLoadedConfigs();
        for (const auto& config : loadedConfigs) {
          xdp::XclbinInfo* xclbin = config->getPlXclbin();
          if (!xclbin)
            continue;
          for (const auto& memory : xclbin->pl.memoryInfo) {
            std::string memName = memory.second->spTag ;

            fout << "MEMORY_USAGE," << device->getUniqueDeviceName() << "|"
                 << memName << "," << memory.second->used << ",\n" ;
          }
        }
      }
    }
  }

  static void PLRAMDevice(xdp::VPDatabase* db, std::ofstream& fout)
  {
    bool hasPLRAM = false ;

    if (xdp::getFlowMode() == xdp::SW_EMU) {
      hasPLRAM = true ;
    }
    else {
      auto deviceInfos = db->getStaticInfo().getDeviceInfos();
      for (auto device : deviceInfos) {
        auto& loadedConfigs = device->getLoadedConfigs();
        for (const auto& config : loadedConfigs) {
          xdp::XclbinInfo* xclbin = config->getPlXclbin();
          if (!xclbin)
            continue;
          for (const auto& memory : xclbin->pl.memoryInfo) {
            if (memory.second->spTag.find("PLRAM") != std::string::npos) {
              hasPLRAM = true ;
              break ;
            }
          }
          if (hasPLRAM) break ;
        }
        if (hasPLRAM) break ;
      }
    }

    fout << "PLRAM_DEVICE,all," << hasPLRAM << ",\n" ;
  }

  static void HBMDevice(xdp::VPDatabase* db, std::ofstream& fout)
  {
    bool hasHBM = false ;

    if (xdp::getFlowMode() == xdp::SW_EMU) {
      // In software emulation we have to search the name for known
      //  platforms that have HBM
      std::string deviceName =
        db->getStaticInfo().getSoftwareEmulationDeviceName() ;
      if (deviceName.find("u280") != std::string::npos ||
          deviceName.find("u50")  != std::string::npos) {
        hasHBM = true ;
      }
    }
    else {
      auto deviceInfos = db->getStaticInfo().getDeviceInfos() ;
      for (auto device : deviceInfos) {
        auto& loadedConfigs = device->getLoadedConfigs();
        for (const auto& config : loadedConfigs) {
          xdp::XclbinInfo* xclbin = config->getPlXclbin();
          if (!xclbin)
            continue;
          for (const auto& memory : xclbin->pl.memoryInfo) {
            if (memory.second->spTag.find("HBM") != std::string::npos) {
              hasHBM = true ;
              break ;
            }
          }
          if (hasHBM) break ;
        }
        if (hasHBM) break ;
      }
    }

    fout << "HBM_DEVICE,all," << hasHBM << ",\n" ;
  }

  static void KDMADevice(xdp::VPDatabase* db, std::ofstream& fout)
  {
    bool hasKDMA = false ;

    // There currently isn't any meta-data that can tell us if the
    //  device has a KDMA, so we are relying on known platforms.

    if (xdp::getFlowMode() == xdp::SW_EMU) {
      std::string deviceName =
        db->getStaticInfo().getSoftwareEmulationDeviceName() ;
      if (deviceName.find("xilinx_u200_xdma") != std::string::npos ||
          deviceName.find("xilinx_vcu1525_xdma") != std::string::npos) {
        hasKDMA = true ;
      }
    }
    else {
      auto deviceInfos = db->getStaticInfo().getDeviceInfos() ;
      for (auto device : deviceInfos) {
        std::string deviceName = device->deviceName ;
        if (deviceName.find("xilinx_u200_xdma") != std::string::npos ||
            deviceName.find("xilinx_vcu1525_xdma") != std::string::npos) {
          hasKDMA = true ;
          break ;
        }
      }
    }

    fout << "KDMA_DEVICE,all," << hasKDMA << ",\n" ;
  }

  static void P2PDevice(xdp::VPDatabase* db, std::ofstream& fout)
  {
    bool hasP2P = false ;

    if (xdp::getFlowMode() == xdp::SW_EMU) {
      std::string deviceName =
        db->getStaticInfo().getSoftwareEmulationDeviceName() ;
      if (deviceName.find("xilinx_u200_xdma")    != std::string::npos ||
          deviceName.find("xilinx_u250_xdma")    != std::string::npos ||
          deviceName.find("samsung")             != std::string::npos ||
          deviceName.find("xilinx_vcu1525_xdma") != std::string::npos) {
        hasP2P = true ;
      }
    }
    else {
      auto deviceInfos = db->getStaticInfo().getDeviceInfos() ;
      for (auto device : deviceInfos) {
        std::string deviceName = device->deviceName ;
        if (deviceName.find("xilinx_u200_xdma")    != std::string::npos ||
            deviceName.find("xilinx_u250_xdma")    != std::string::npos ||
            deviceName.find("samsung")             != std::string::npos ||
            deviceName.find("xilinx_vcu1525_xdma") != std::string::npos) {
          hasP2P = true ;
          break ;
        }
      }
    }

    fout << "P2P_DEVICE,all," << hasP2P << ",\n" ;
  }

  static void P2PHostTransfers(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::opencl_trace)) return ;

    uint64_t hostP2PTransfers = db->getStats().getNumHostP2PTransfers() ;
    fout << "P2P_HOST_TRANSFERS,host," << hostP2PTransfers << ",\n" ;
  }

  static void portBitWidth(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (xdp::getFlowMode() == xdp::SW_EMU) {
      std::vector<std::string> portBitWidths =
        db->getStaticInfo().getSoftwareEmulationPortBitWidths();
      for (const auto& width : portBitWidths)
        fout << "PORT_BIT_WIDTH," << width << ",\n";
      return;
    }

    // Hardware and HW-EMU

    auto deviceInfos = db->getStaticInfo().getDeviceInfos() ;
    for (auto device : deviceInfos) {

      auto& loadedConfigs = device->getLoadedConfigs();
      for (const auto& config : loadedConfigs) {
        xdp::XclbinInfo* xclbin = config->getPlXclbin();
        if (!xclbin)
          continue;
        for (const auto& cu : xclbin->pl.cus) {
          std::vector<uint32_t>* aimIds = cu.second->getAIMs() ;
          std::vector<uint32_t>* asmIds = cu.second->getASMs() ;

          for (auto aim : (*aimIds)) {
            xdp::Monitor* monitor =
              db->getStaticInfo().getAIMonitor(device->deviceId, xclbin, aim);
            if (monitor->cuPort) {
              fout << "PORT_BIT_WIDTH," << cu.second->getName() << "/"
                   << monitor->cuPort->name << ","
                   << monitor->cuPort->bitWidth << ",\n";
            }
          }

          for (auto asmId : (*asmIds)) {
            xdp::Monitor* monitor =
              db->getStaticInfo().getASMonitor(device->deviceId,xclbin,asmId);
            if (monitor->cuPort) {
              fout << "PORT_BIT_WIDTH," << cu.second->getName() << "/"
                   << monitor->cuPort->name << ","
                   << monitor->cuPort->bitWidth << ",\n" ;
            }
          }
        }
      }
    }
  }

  void kernelCount(xdp::VPDatabase* db, std::ofstream& fout)
  {
    std::map<std::string, uint64_t> kernelCounts ;

    if (xdp::getFlowMode() == xdp::SW_EMU) {
      kernelCounts = db->getStaticInfo().getSoftwareEmulationCUCounts() ;
    }
    else {
      auto deviceInfos = db->getStaticInfo().getDeviceInfos() ;
      for (auto device : deviceInfos) {
        auto& loadedConfigs = device->getLoadedConfigs();
        for (const auto& config : loadedConfigs) {
          xdp::XclbinInfo* xclbin = config->getPlXclbin();
          if (!xclbin)
            continue;
          for (const auto& cu : xclbin->pl.cus) {
            std::string kernelName = cu.second->getKernelName() ;
            if (kernelCounts.find(kernelName) == kernelCounts.end()) {
              kernelCounts[kernelName] = 1 ;
            }
            else {
              kernelCounts[kernelName] += 1 ;
            }
          }
        }
      }
    }

    for (const auto& kernel : kernelCounts) {
      fout << "KERNEL_COUNT," << kernel.first << "," << kernel.second << ",\n" ;
    }
  }

  static void objectsReleased(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::opencl_trace)) return ;

    uint64_t numReleased = db->getStats().getNumOpenCLObjectsReleased();

    fout << "OBJECTS_RELEASED,all," << numReleased << ",\n" ;
  }

  static void CUContextEn(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::opencl_trace)) return ;

    bool isContextEnabled = db->getStats().getContextEnabled() ;

    fout << "CU_CONTEXT_EN,all," << (uint64_t)(isContextEnabled) << ",\n" ;
  }

  static void traceMemory(xdp::VPDatabase* db, std::ofstream& fout)
  {
    std::string memType = "N/A" ;

    if ((xdp::getFlowMode() != xdp::SW_EMU) &&
        (xdp::getFlowMode() != xdp::HW_EMU)) {
      auto deviceInfos = db->getStaticInfo().getDeviceInfos() ;

      for (auto device : deviceInfos) {
        auto& loadedConfigs = device->getLoadedConfigs();
        for (const auto& config : loadedConfigs) {
          xdp::XclbinInfo* xclbin = config->getPlXclbin();
          if (!xclbin)
            continue;
          if (xclbin->pl.usesTs2mm) {
            memType = "TS2MM" ;
            break ;
          } else if (xclbin->pl.usesFifo) {
            memType = "FIFO" ;
            break ;
          }
        }
      }
    }
    fout << "TRACE_MEMORY,all," << memType << ",\n" ;
  }

  static void maxParallelKernelEnqueues(xdp::VPDatabase* db,
                                        std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::opencl_trace)) return ;

    auto maxExecs = db->getStats().getAllMaxExecutions();

    for (const auto& mExec : maxExecs) {
      fout << "MAX_PARALLEL_KERNEL_ENQUEUES," << mExec.first << ","
           << mExec.second << ",\n" ;
    }
  }

  static void commandQueueOOO(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::opencl_trace)) return ;

    auto commandQueueInfo = db->getStats().getCommandQueuesAreOOO() ;

    for (const auto& cq : commandQueueInfo) {
      fout << "COMMAND_QUEUE_OOO," << cq.first << "," << cq.second << ",\n" ;
    }
  }

  static void PLRAMSizeBytes(xdp::VPDatabase* db, std::ofstream& fout)
  {
    auto deviceInfos = db->getStaticInfo().getDeviceInfos() ;
    bool done = false ;

    for (auto device : deviceInfos) {
      auto& loadedConfigs = device->getLoadedConfigs();
      for (const auto& config : loadedConfigs) {
        xdp::XclbinInfo* xclbin = config->getPlXclbin();
        if (!xclbin)
          continue;
        for (const auto& memory : xclbin->pl.memoryInfo) {
          if (memory.second->spTag.find("PLRAM") != std::string::npos) {
            fout << "PLRAM_SIZE_BYTES,"
                 << device->getUniqueDeviceName()
                 << "," << memory.second->size*1024 << ",\n" ;
            done = true ;
            // To match old flow and tools, print PLRAM_SIZE_BYTES for
            //  first match only.
            break ;
          }
        }
        if (done) break ;
      }
      if (done) break ;
    }
  }

  static void kernelBufferInfo(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::opencl_trace)) return ;

    // This reports the memory bank, argument, alignment, and size
    //  of each buffer.

    for (auto& iter : db->getStats().getBufferInfo()) {
      for (auto& info : iter.second) {
        fout << "KERNEL_BUFFER_INFO," << info << ",\n" ;
      }
    }
  }

  static void traceBufferFull(xdp::VPDatabase* db, std::ofstream& fout)
  {
    auto deviceInfos = db->getStaticInfo().getDeviceInfos() ;
    for (auto device : deviceInfos) {
      bool full = db->getDynamicInfo().isPLTraceBufferFull(device->deviceId);
      fout << "TRACE_BUFFER_FULL,"
           << device->getUniqueDeviceName() << ","
           << (full ? "true" : "false")
           << "\n" ; // Should there be a comma at the end?
    }
  }

  static void memoryTypeBitWidth(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (xdp::getFlowMode() == xdp::SW_EMU) {
      std::string deviceName =
        db->getStaticInfo().getSoftwareEmulationDeviceName() ;
      fout << "MEMORY_TYPE_BIT_WIDTH," << deviceName
           << "|HBM," << 256 << ",\n" ;
      fout << "MEMORY_TYPE_BIT_WIDTH," << deviceName
           << "|DDR," << 512 << ",\n" ;
      fout << "MEMORY_TYPE_BIT_WIDTH," << deviceName
           << "|PLRAM," << 512 << ",\n" ;
    }
    else {
      auto deviceInfos = db->getStaticInfo().getDeviceInfos() ;

      for (auto device : deviceInfos) {
        std::string deviceName = device->getUniqueDeviceName() ;

        if (device->isEdgeDevice) {
          fout << "MEMORY_TYPE_BIT_WIDTH," << deviceName
               << "|DDR," << 64 << ",\n" ;
        }
        else {
          fout << "MEMORY_TYPE_BIT_WIDTH," << deviceName
               << "|HBM," << 256 << ",\n" ;
          fout << "MEMORY_TYPE_BIT_WIDTH," << deviceName
               << "|DDR," << 512 << ",\n" ;
          fout << "MEMORY_TYPE_BIT_WIDTH," << deviceName
               << "|PLRAM," << 512 << ",\n" ;
        }
      }
    }
  }

  static void bufferRdActiveTimeMs(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::opencl_trace)) return ;

    fout << "BUFFER_RD_ACTIVE_TIME_MS,all,"
         << (double)(db->getStats().getTotalHostReadTime()) / 1e06
         << ",\n" ;
  }

  static void bufferWrActiveTimeMs(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::opencl_trace)) return ;

    fout << "BUFFER_WR_ACTIVE_TIME_MS,all,"
         << (double)(db->getStats().getTotalHostWriteTime()) / 1e06
         << ",\n" ;
  }

  static void bufferTxActiveTimeMs(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::opencl_trace)) return ;

    fout << "BUFFER_TX_ACTIVE_TIME_MS,all,"
         << (double)(db->getStats().getTotalBufferTxTime()) / 1e06
         << ",\n" ;
  }

  static void applicationRunTimeMs(xdp::VPDatabase* db, std::ofstream& fout)
  {
    uint64_t startTime = db->getStaticInfo().getApplicationStartTime() ;
    uint64_t endTime = xrt_core::time_ns() ; 

    fout << "APPLICATION_RUN_TIME_MS,all," 
         << (double)(endTime - startTime) / 1e06
         << ",\n" ;
  }

  static void totalKernelRunTimeMs(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::device_offload)) return ;

    double firstKernelStartTime = db->getStats().getFirstKernelStartTime() ;
    double lastKernelEndTime = db->getStats().getLastKernelEndTime() ;

    fout << "TOTAL_KERNEL_RUN_TIME_MS,all,"
         << lastKernelEndTime - firstKernelStartTime << ",\n" ;
  }

  static void aieCounterResources(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::aie_profile)) return;

    auto deviceInfos = db->getStaticInfo().getDeviceInfos();
    for (auto device : deviceInfos) {
      auto coreCounters =
        db->getStaticInfo().getAIECoreCounterResources(device->deviceId);
      if (coreCounters != nullptr) {
        for (auto const& counter : *coreCounters) {
          fout << "AIE_CORE_COUNTER_RESOURCES," << counter.first << ","
               << counter.second << ",\n";
        }
      }

      auto memoryCounters =
        db->getStaticInfo().getAIEMemoryCounterResources(device->deviceId);
      if (memoryCounters != nullptr) {
        for (auto const& counter : *memoryCounters) {
          fout << "AIE_MEMORY_COUNTER_RESOURCES," << counter.first << ","
               << counter.second << ",\n";
        }
      }

      auto interfaceCounters =
        db->getStaticInfo().getAIEShimCounterResources(device->deviceId);
      if (interfaceCounters != nullptr) {
        for (auto const& counter : *interfaceCounters) {
          fout << "AIE_INTERFACE_COUNTER_RESOURCES," << counter.first << ","
               << counter.second << ",\n";
        }
      }
      
      auto memTileCounters =
        db->getStaticInfo().getAIEMemTileCounterResources(device->deviceId);
      if (memTileCounters != nullptr) {
        for (auto const& counter : *memTileCounters) {
          fout << "AIE_MEM_TILE_COUNTER_RESOURCES," << counter.first << ","
               << counter.second << ",\n";
        }
      }
    }
  }

  static void aieTraceEvents(xdp::VPDatabase* db, std::ofstream& fout)
  {
    if (!db->infoAvailable(xdp::info::aie_trace)) return ;

    auto deviceInfos = db->getStaticInfo().getDeviceInfos() ;
    for (auto device : deviceInfos) {
      auto coreEvents =
        db->getStaticInfo().getAIECoreEventResources(device->deviceId) ;
      if (coreEvents != nullptr) {
        for (auto const& coreEvent : *coreEvents) {
          fout << "AIE_CORE_EVENT_RESOURCES," << coreEvent.first << ","
               << coreEvent.second << ",\n" ;
        }
      }

      auto memoryEvents =
        db->getStaticInfo().getAIEMemoryEventResources(device->deviceId) ;
      if (memoryEvents != nullptr) {
        for (auto const& memoryEvent : *memoryEvents) {
          fout << "AIE_MEMORY_EVENT_RESOURCES," << memoryEvent.first << ","
               << memoryEvent.second << ",\n" ;
        }
      }

      auto interfaceEvents =
        db->getStaticInfo().getAIEShimEventResources(device->deviceId) ;
      if (interfaceEvents != nullptr) {
        for (auto const& interfaceEvent : *interfaceEvents) {
          fout << "AIE_INTERFACE_EVENT_RESOURCES," << interfaceEvent.first << ","
               << interfaceEvent.second << ",\n" ;
        }
      }

      auto memTileEvents =
        db->getStaticInfo().getAIEMemTileEventResources(device->deviceId) ;
      if (memTileEvents != nullptr) {
        for (auto const& memTileEvent : *memTileEvents) {
          fout << "AIE_MEM_TILE_EVENT_RESOURCES," << memTileEvent.first << ","
               << memTileEvent.second << ",\n" ;
        }
      }
    }
  }

} // end anonymous namespace

namespace xdp {

  GuidanceRules::GuidanceRules() : iniParameters()
  {
    // Add the rules that apply to all executions
    rules.push_back(deviceExecTime) ;
    rules.push_back(cuCalls);
    rules.push_back(numMonitors);
    rules.push_back(memoryUsage);
    rules.push_back(PLRAMDevice);
    rules.push_back(HBMDevice) ;
    rules.push_back(KDMADevice) ;
    rules.push_back(P2PDevice) ;
    rules.push_back(portBitWidth) ;
    rules.push_back(kernelCount) ;
    rules.push_back(traceMemory) ;
    rules.push_back(PLRAMSizeBytes) ;
    rules.push_back(traceBufferFull) ;
    rules.push_back(memoryTypeBitWidth) ;
    rules.push_back(applicationRunTimeMs) ;

    // Add the OpenCL specific rules if OpenCL information is available
    rules.push_back(migrateMem) ;
    rules.push_back(P2PHostTransfers) ;
    rules.push_back(objectsReleased) ;
    rules.push_back(CUContextEn) ;
    rules.push_back(maxParallelKernelEnqueues);
    rules.push_back(commandQueueOOO) ;
    rules.push_back(kernelBufferInfo) ;
    rules.push_back(bufferRdActiveTimeMs) ;
    rules.push_back(bufferWrActiveTimeMs) ;
    rules.push_back(bufferTxActiveTimeMs) ;
    rules.push_back(totalKernelRunTimeMs) ;

    // Add the AIE information if available
    rules.push_back(aieCounterResources) ;
    rules.push_back(aieTraceEvents) ;
  }

  GuidanceRules::~GuidanceRules()
  {
  }

  void GuidanceRules::write(VPDatabase* db, std::ofstream& fout)
  {
    // Dump the header
    fout << "Guidance Parameters\n" ;
    fout << "Parameter,Element,Value,\n" ;

    for (auto& rule : rules) {
      rule(db, fout) ;
    }

    iniParameters.write(fout) ;
  }

} // end namespace xdp
