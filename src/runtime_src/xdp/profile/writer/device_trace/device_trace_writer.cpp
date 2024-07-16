/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#define XDP_PLUGIN_SOURCE

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/device_events.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/static_info/xclbin_info.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/device_trace/device_trace_writer.h"

namespace xdp {

  DeviceTraceWriter::DeviceTraceWriter(const char* filename, uint64_t devId, 
                                       const std::string& version,
                                       const std::string& creationTime,
                                       const std::string& xrtV,
                                       const std::string& toolV)
    : VPTraceWriter(filename, version, creationTime, 9 /* ns */),
      xrtVersion(xrtV),
      toolVersion(toolV),
      deviceId(devId)
  {
  }

  DeviceTraceWriter::~DeviceTraceWriter()
  {
  }

  void DeviceTraceWriter::writeHeader()
  {
    VPTraceWriter::writeHeader();
    std::string targetRun;
    if(xdp::getFlowMode() == xdp::HW) {
      targetRun = "System Run";
    } else if(xdp::getFlowMode() == xdp::HW_EMU) {
      targetRun = "Hardware Emulation";
    }
    fout << "TraceID," << traceID << "\n"
         << "XRT  Version," << xrtVersion  << "\n"
         << "Tool Version," << toolVersion << "\n"
         << "Platform," << (db->getStaticInfo()).getDeviceName(deviceId) << "\n"
         << "Target," << targetRun << "\n";
  }

  // This function writes the portion of the structure that is true for
  //  all xclbins loaded on the device.  Things like shell-specific 
  //  monitors go here.
  void DeviceTraceWriter::writeDeviceStructure()
  {
    uint64_t numKDMA = (db->getStaticInfo()).getKDMACount(deviceId);
    if(numKDMA) {
#if 0
      fout << "Group_Start,KDMA\n";
      for (unsigned int i = 0 ; i < numKDMA ; ++i)
      {
              fout << "Dynamic_Row," << ++rowCount << ",Read, ,KERNEL_READ\n";
              fout << "Dynamic_Row," << ++rowCount << ",Write, ,KERNEL_WRITE\n";
      }
      fout << "Group_End,KDMA\n";
#endif
    }
  }

  void DeviceTraceWriter::writeLoadedXclbinsStructure()
  {
    uint32_t rowCount = 0;
    //std::vector<ConfigInfo*> configs =
    auto& configs =
      (db->getStaticInfo()).getLoadedConfigs(deviceId);
    
    for (const auto& config : configs) {
        std::string configXclbinNames = config->getXclbinNames();
        fout << "Group_Start," << configXclbinNames << "\n";
        XclbinInfo* xclbin = config->getPlXclbin();
        if (!xclbin)
          continue;
        writeSingleXclbinStructure(xclbin, rowCount);
        fout << "Group_End," << configXclbinNames << "\n";
    }
  }

  void DeviceTraceWriter::writeSingleXclbinStructure(XclbinInfo* xclbin,
                                                     uint32_t& rowCount)
  {
    // Create structure for all CUs in the xclbin
    for (const auto& iter : xclbin->pl.cus) {
      ComputeUnitInstance* cu = iter.second;
      fout << "Group_Start,Compute Unit " << cu->getName();

      if(-1 == cu->getAccelMon() && !(cu->getDataTransferTraceEnabled())
                                 && !(cu->getStreamTraceEnabled())) {
        fout << " - No Trace";
      }

      fout << ",Activity in accelerator "<< cu->getKernelName() 
           << ":" << cu->getName() << "\n";

      writeCUExecutionStructure(xclbin, cu, rowCount);
      writeCUMemoryTransfersStructure(xclbin, cu, rowCount);
      writeCUStreamTransfersStructure(xclbin, cu, rowCount);

      fout << "Group_End," << cu->getName() << "\n";
    }
    // Create structure for all floating monitors not attached to a CU, and enabled for trace
    writeFloatingMemoryTransfersStructure(xclbin, rowCount);
    writeFloatingStreamTransfersStructure(xclbin, rowCount);
  }

  void DeviceTraceWriter::writeCUExecutionStructure(XclbinInfo* xclbin,
                                                    ComputeUnitInstance* cu,
                                                    uint32_t& rowCount)
  {
    if(-1 == cu->getAccelMon()) { 
      // No trace enabled AMs are present
      return;
    } 
    fout << "Dynamic_Row_Summary," << ++rowCount
         << ",Executions,Execution in accelerator " 
         << cu->getName() << "\n";

    if(xdp::getFlowMode() == xdp::HW_EMU) {
      size_t pos = xclbin->name.find(".xclbin");
      fout << "Optional_Function_Internal,User Functions,Function activity in accelerator " << cu->getName() 
           << "," << rowCount
           << "," << (db->getStaticInfo()).getDeviceName(deviceId) << "-0"
           << "," << xclbin->name.substr(0, pos)
           << "," << cu->getKernelName()
           << "," << cu->getName() << "\n";
    }

    std::pair<XclbinInfo*, int32_t> index =
      std::make_pair(xclbin, cu->getIndex());
    cuBucketIdMap[index] = rowCount;

    // Generate wave group for Kernel Stall if Stall monitoring is enabled in CU
    if (cu->getStallEnabled()) {
      fout << "Group_Summary_Start,Stall,Stalls in accelerator " << cu->getName() << "\n";
      fout << "Static_Row," << (rowCount + KERNEL_STALL_EXT_MEM - KERNEL)  << ",External Memory Stall, Stalls from accessing external memory" << "\n";
      fout << "Static_Row," << (rowCount + KERNEL_STALL_DATAFLOW - KERNEL) << ",Intra-Kernel Dataflow Stall,Stalls from dataflow streams inside compute unit" << "\n";
      fout << "Static_Row," << (rowCount + KERNEL_STALL_PIPE - KERNEL) << ",Inter-Kernel Pipe Stall,Stalls from accessing pipes between kernels" << "\n";
      fout << "Group_End,Stall\n";

      rowCount += (KERNEL_STALL_PIPE - KERNEL);
    }
  }

  void DeviceTraceWriter::writeCUMemoryTransfersStructure(XclbinInfo* xclbin, ComputeUnitInstance* cu, uint32_t& rowCount)
  {
    // Generate Wave group for Read/Write if data transfer monitoring is enabled
    if (!(cu->getDataTransferTraceEnabled()))
      return;

    std::vector<uint32_t>* cuAIMs = cu->getAIMsWithTrace() ;
    for (auto cuAIM : *cuAIMs) {
      Monitor* aim = (db->getStaticInfo()).getAIMonitor(deviceId, xclbin, cuAIM);
      if (nullptr == aim)
        continue;

      std::pair<XclbinInfo*, uint32_t> index = std::make_pair(xclbin, cuAIM);
      aimBucketIdMap[index] = ++rowCount;

      size_t pos = aim->name.find('/');
      std::string portAndArgs = (std::string::npos != pos) ? aim->name.substr(pos+1) : aim->name;
      if (aim->cuPort && !aim->cuPort->args.empty()) {
        portAndArgs += "(";
        bool first = true;
        for (auto& arg : aim->cuPort->args) {
          if (!first)
            portAndArgs += "|";
          portAndArgs += arg;
          first = false;
        }
        portAndArgs += ")";
      }

      // Data Transfers
      fout << "Group_Start," << portAndArgs << ",Data Transfers between " << cu->getName() << " and Global Memory over read and write channels of " << aim->name << "\n";
      fout << "Static_Row," << rowCount   << ",Read Channel,Read Data Transfers " << "\n";
      fout << "Static_Row," << ++rowCount << ",Write Channel,Write Data Transfers " << "\n";
      fout << "Group_End," << portAndArgs << "\n";
    }
  }

  void DeviceTraceWriter::writeCUStreamTransfersStructure(XclbinInfo* xclbin, ComputeUnitInstance* cu, uint32_t& rowCount)
  {
    // Generate Wave group for stream data transfers if enabled
    if (!(cu->getStreamTraceEnabled()))
      return;

    std::vector<uint32_t>* cuASMs = cu->getASMsWithTrace();
    for (auto cuASM : *cuASMs) {
      Monitor* ASM = (db->getStaticInfo()).getASMonitor(deviceId, xclbin, cuASM);
      if (nullptr == ASM)
        continue;

      std::pair<XclbinInfo*, uint32_t> index = std::make_pair(xclbin, cuASM);
      asmBucketIdMap[index] = ++rowCount;

      // KERNEL_STREAM_READ/WRITE
      fout << "Group_Start," << ASM->name << ",AXI Stream transaction over " << ASM->name << "\n";
      fout << "Static_Row," << rowCount << ",Stream Activity,AXI Stream transactions over " << ASM->name << "\n";
      fout << "Static_Row," << ++rowCount << ",Link Stall" << "\n";
      fout << "Static_Row," << ++rowCount << ",Link Starve" << "\n";
      fout << "Group_End," << ASM->name << "\n";
    }
  }

  void DeviceTraceWriter::writeFloatingMemoryTransfersStructure(XclbinInfo* xclbin, uint32_t& rowCount)
  {
    if (!(db->getStaticInfo().hasFloatingAIMWithTrace(deviceId, xclbin)))
      return;

    fout << "Group_Start,AXI Memory Monitors,Read/Write data transfers over AXI Memory Mapped connection \n";

    // Go through all of the AIMs in this xclbin to find the floating ones
    std::vector<Monitor*>* aims =
      db->getStaticInfo().getAIMonitors(deviceId, xclbin);

    size_t i = 0;
    for (auto aim : *aims) {
      if (nullptr == aim)
        continue;
      if (-1 != aim->cuIndex) {
        // not a floating AIM, must have been covered in CU section
        ++i;
        continue;
      }
      std::pair<XclbinInfo*, uint32_t> index = std::make_pair(xclbin, static_cast<uint32_t>(i));
      aimBucketIdMap[index] = ++rowCount;

      std::string portAndArgs = aim->name;
      if (aim->cuPort && !aim->cuPort->args.empty()) {
        portAndArgs += "(";
        bool first = true;
        for (auto& arg : aim->cuPort->args) {
          if (!first)
            portAndArgs += "|";
          portAndArgs += arg;
          first = false;
        }
        portAndArgs += ")";
      }

      fout << "Group_Start," << portAndArgs << ",Data Transfers over read and write channels of AXI Memory Mapped " << aim->name << "\n";
      fout << "Static_Row,"  << rowCount   << ",Read Channel,Read Data Transfers " << "\n";
      fout << "Static_Row,"  << ++rowCount << ",Write Channel,Write Data Transfers " << "\n";
      fout << "Group_End,"   << portAndArgs << "\n";
      i++;
    }
    fout << "Group_End,AXI Memory Monitors" << "\n";
  }

  void DeviceTraceWriter::writeFloatingStreamTransfersStructure(XclbinInfo* xclbin, uint32_t& rowCount)
  {
    if (!(db->getStaticInfo()).hasFloatingASMWithTrace(deviceId, xclbin))
      return;

    fout << "Group_Start,AXI Stream Monitors,Data transfers over AXI Stream connection " << "\n";

    std::vector<Monitor*>* asms =
      db->getStaticInfo().getASMonitors(deviceId, xclbin);

    size_t i = 0;
    for (auto asM : *asms) {
      if (nullptr == asM)
        continue;
      if (-1 != asM->cuIndex) {
        // Not a floating ASM.  Must have been covered in CU section
        ++i;
        continue;
      }

      std::pair<XclbinInfo*, uint32_t> index = std::make_pair(xclbin, static_cast<uint32_t>(i));
      asmBucketIdMap[index] = ++rowCount;
      fout << "Group_Start," << asM->name << ",AXI Stream transactions over " << asM->name << "\n";
      fout << "Static_Row," << rowCount << ",Stream Activity,AXI Stream transactions over " << asM->name << "\n";
      fout << "Static_Row," << ++rowCount << ",Link Stall" << "\n";
      fout << "Static_Row," << ++rowCount << ",Link Starve" << "\n";
      fout << "Group_End," << asM->name << "\n";
      i++;
    }
    fout << "Group_End,AXI Stream Monitors\n";
  }

  void DeviceTraceWriter::writeStructure()
  {
    fout << "STRUCTURE\n";
    
    // Use the database's "static" information to discover how many
    //  kernels, compute units, etc. this device has.  Then, use that
    //  to build up the structure of the file we are generating
    
    std::string deviceName = (db->getStaticInfo()).getDeviceName(deviceId);
    fout << "Group_Start," << deviceName << "\n";
    writeDeviceStructure();
    writeLoadedXclbinsStructure();
    fout << "Group_End," << deviceName << "\n";
  }

  void DeviceTraceWriter::writeStringTable()
  {
    fout << "MAPPING\n";
    (db->getDynamicInfo()).dumpStringTable(fout);
  }

  void DeviceTraceWriter::writeTraceEvents()
  {
    fout << "EVENTS\n";
    auto DeviceEvents = db->getDynamicInfo().moveDeviceEvents(deviceId);

    auto& loadedConfigs =
      (db->getStaticInfo()).getLoadedConfigs(deviceId);
    if (loadedConfigs.size() <= 0) {
      return;
    }

    int configIndex = 0;
    ConfigInfo* config = loadedConfigs[configIndex].get();
    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return;

    for(auto& e : DeviceEvents) {
      VTFDeviceEvent* deviceEvent = dynamic_cast<VTFDeviceEvent*>(e.get());
      if(!deviceEvent)
        continue;
      
      int32_t cuId = deviceEvent->getCUId();
      VTFEventType eventType = deviceEvent->getEventType();
      if (XCLBIN_END == eventType) {
        // If we hit the end of an xclbin's execution, then increment xclbins
        configIndex++;
        if(configIndex < static_cast<int>(loadedConfigs.size())) {
          config = loadedConfigs[configIndex].get();
          xclbin = config->getPlXclbin();
        }
        // TODO: Check if expect invalid PL xclbin here?

      } else if (KERNEL == eventType) {
        KernelEvent* kernelEvent = dynamic_cast<KernelEvent*>(deviceEvent);
        if (kernelEvent == nullptr)
          continue; // Coverity - In case dynamic cast fails
        std::pair<XclbinInfo*, int32_t> index =
          std::make_pair(xclbin, cuId);
        kernelEvent->dump(fout, cuBucketIdMap[index] + eventType - KERNEL);
        // Also output the tool tips
        for (const auto& iter : xclbin->pl.cus) {
          ComputeUnitInstance* cu = iter.second;
          if (cu->getAccelMon() == cuId) {
            fout << "," << db->getDynamicInfo().addString(cu->getKernelName());
            fout << "," << db->getDynamicInfo().addString(cu->getName());
          }
        }
        fout << "\n";
      } else if(KERNEL_STALL_EXT_MEM == eventType
                || KERNEL_STALL_DATAFLOW == eventType
                || KERNEL_STALL_PIPE == eventType) {
        std::pair<XclbinInfo*, int32_t> index =
          std::make_pair(xclbin, cuId);
        deviceEvent->dump(fout, cuBucketIdMap[index] + eventType - KERNEL);
      } else {
        // Memory or Stream Acceses
        uint32_t monId = deviceEvent->getMonitorId();
        DeviceMemoryAccess* memoryEvent = dynamic_cast<DeviceMemoryAccess*>(e.get());
        if (memoryEvent) {
          std::pair<XclbinInfo*, uint32_t> index =std::make_pair(xclbin, monId);
          deviceEvent->dump(fout, aimBucketIdMap[index] + eventType - KERNEL_READ);
          continue;
        }
        DeviceStreamAccess* streamEvent = dynamic_cast<DeviceStreamAccess*>(e.get());
        if (streamEvent) {
          std::pair<XclbinInfo*, uint32_t> index = std::make_pair(xclbin, monId);
          if (KERNEL_STREAM_READ == eventType || KERNEL_STREAM_READ_STALL == eventType
                                              || KERNEL_STREAM_READ_STARVE == eventType) {
            deviceEvent->dump(fout, asmBucketIdMap[index] + eventType - KERNEL_STREAM_READ);
          } else {
            deviceEvent->dump(fout, asmBucketIdMap[index] + eventType - KERNEL_STREAM_WRITE);
          }
          continue;
        }
        // host read/write ??
      }
    }

  }

  void DeviceTraceWriter::writeDependencies()
  {
    fout << "DEPENDENCIES\n";
    // No dependencies in device events
  }

  bool DeviceTraceWriter::traceEventsExist()
  {
    return (db->getDynamicInfo()).deviceEventsExist(deviceId);
  }

  bool DeviceTraceWriter::write(bool openNewFile)
  {
    if (openNewFile && !traceEventsExist())
      return false;

    initialize();

    writeHeader();
    fout << "\n";
    writeStructure();
    fout << "\n";
    writeStringTable();
    fout << "\n";
    writeTraceEvents();
    fout << "\n";
    writeDependencies();
    fout << "\n";

    fout.flush();

    if (openNewFile) {
      switchFiles();
      db->getStaticInfo().addOpenedFile(getcurrentFileName(), "VP_TRACE");
    }
    return true;
  }

  void DeviceTraceWriter::initialize()
  {
    auto& loadedConfigs =
      (db->getStaticInfo()).getLoadedConfigs(deviceId);

    for (const auto& config : loadedConfigs) {
      XclbinInfo* xclbin = config->getPlXclbin();
      if (!xclbin)
        continue;

      for (const auto& iter : xclbin->pl.cus) {
        ComputeUnitInstance* cu = iter.second;
        db->getDynamicInfo().addString(cu->getKernelName());
        db->getDynamicInfo().addString(cu->getName());
      }
    }
  }

} // end namespace xdp
