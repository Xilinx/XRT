/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include "xdp/profile/writer/device_trace/device_trace_writer.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/device_events.h"
#include "xdp/profile/plugin/vp_base/utility.h"

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
    VPTraceWriter::writeHeader() ;
    std::string targetRun;
    if(xdp::getFlowMode() == xdp::HW) {
      targetRun = "System Run";
    } else if(xdp::getFlowMode() == xdp::HW_EMU) {
      targetRun = "Hardware Emulation";
    }
    fout << "TraceID," << traceID << std::endl
         << "XRT  Version," << xrtVersion  << std::endl
         << "Tool Version," << toolVersion << std::endl
         << "Platform," << (db->getStaticInfo()).getDeviceName(deviceId) << std::endl
         << "Target," << targetRun << std::endl;
  }

  // This function writes the portion of the structure that is true for
  //  all xclbins loaded on the device.  Things like shell-specific 
  //  monitors go here.
  void DeviceTraceWriter::writeDeviceStructure()
  {
    uint64_t numKDMA = (db->getStaticInfo()).getKDMACount(deviceId) ;
    if(numKDMA) {
#if 0
      fout << "Group_Start,KDMA" << std::endl ;
      for (unsigned int i = 0 ; i < numKDMA ; ++i)
      {
              fout << "Dynamic_Row," << ++rowCount << ",Read, ,KERNEL_READ" << std::endl;
              fout << "Dynamic_Row," << ++rowCount << ",Write, ,KERNEL_WRITE" << std::endl;
      }
      fout << "Group_End,KDMA" << std::endl ;
#endif
    }
  }

  void DeviceTraceWriter::writeLoadedXclbinsStructure()
  {
    uint32_t rowCount = 0 ;
    std::vector<XclbinInfo*> xclbins =
      (db->getStaticInfo()).getLoadedXclbins(deviceId) ;

    for (auto xclbin : xclbins) {
      fout << "Group_Start," << xclbin->name << std::endl ;
      writeSingleXclbinStructure(xclbin, rowCount) ;
      fout << "Group_End," << xclbin->name << std::endl ;
    }
  }

  void DeviceTraceWriter::writeSingleXclbinStructure(XclbinInfo* xclbin,
                                                     uint32_t& rowCount)
  {
    // Create structure for all CUs in the xclbin
    for (auto iter : xclbin->cus) {
      ComputeUnitInstance* cu = iter.second ;
      fout << "Group_Start,Compute Unit " << cu->getName() 
           << ",Activity in accelerator "<< cu->getKernelName() 
           << ":" << cu->getName() << std::endl ;

      writeCUExecutionStructure(xclbin, cu, rowCount) ;
      writeCUMemoryTransfersStructure(xclbin, cu, rowCount) ;
      writeCUStreamTransfersStructure(xclbin, cu, rowCount) ;

      fout << "Group_End," << cu->getName() << std::endl ;
    }
    // Create structure for all floating monitors not attached to a CU
    writeFloatingMemoryTransfersStructure(xclbin, rowCount) ;
    writeFloatingStreamTransfersStructure(xclbin, rowCount) ;
  }

  void DeviceTraceWriter::writeCUExecutionStructure(XclbinInfo* xclbin,
                                                    ComputeUnitInstance* cu,
                                                    uint32_t& rowCount)
  {
    fout << "Dynamic_Row_Summary," << ++rowCount
         << ",Executions,Execution in accelerator " 
         << cu->getName() << std::endl;

    if(xdp::getFlowMode() == xdp::HW_EMU) {
      size_t pos = xclbin->name.find('.');
      fout << "Optional_Function_Internal,User Functions,Function activity in accelerator " << cu->getName() 
           << "," << rowCount
           << "," << (db->getStaticInfo()).getDeviceName(deviceId) << "-0"
           << "," << xclbin->name.substr(0, pos)
           << "," << cu->getKernelName()
           << "," << cu->getName() << std::endl;
    }

    std::pair<XclbinInfo*, int32_t> index =
      std::make_pair(xclbin, cu->getIndex()) ;
    cuBucketIdMap[index] = rowCount;

    // Generate wave group for Kernel Stall if Stall monitoring is enabled in CU
    if (cu->stallEnabled()) {
      fout << "Group_Summary_Start,Stall,Stalls in accelerator " << cu->getName() << std::endl;
      fout << "Static_Row," << (rowCount + KERNEL_STALL_EXT_MEM - KERNEL)  << ",External Memory Stall, Stalls from accessing external memory" << std::endl;
      fout << "Static_Row," << (rowCount + KERNEL_STALL_DATAFLOW - KERNEL) << ",Intra-Kernel Dataflow Stall,Stalls from dataflow streams inside compute unit" << std::endl;
      fout << "Static_Row," << (rowCount + KERNEL_STALL_PIPE - KERNEL) << ",Inter-Kernel Pipe Stall,Stalls from accessing pipes between kernels" << std::endl;
      fout << "Group_End,Stall" << std::endl;

      rowCount += (KERNEL_STALL_PIPE - KERNEL);
    }
  }

  void DeviceTraceWriter::writeCUMemoryTransfersStructure(XclbinInfo* xclbin, ComputeUnitInstance* cu, uint32_t& rowCount)
  {
    // Generate Wave group for Read/Write if data transfer monitoring is enabled
    if (!(cu->dataTransferEnabled())) return ;

    std::vector<uint32_t>* cuAIMs = cu->getAIMs() ;
    for (auto cuAIM : *cuAIMs) {
      Monitor* aim = (db->getStaticInfo()).getAIMonitor(deviceId, xclbin, cuAIM) ;
      if (nullptr == aim) continue ;

      std::pair<XclbinInfo*, uint32_t> index = std::make_pair(xclbin, cuAIM) ;
      aimBucketIdMap[index] = ++rowCount ;

      size_t pos = aim->name.find('/');
      std::string portAndArgs = (std::string::npos != pos) ? aim->name.substr(pos+1) : aim->name;
      if(!aim->args.empty()) {
        portAndArgs += " (" + aim->args + ")";
      }

      // Data Transfers
      fout << "Group_Start," << portAndArgs << ",Data Transfers between " << cu->getName() << " and Global Memory over read and write channels of " << aim->name << std::endl;
      fout << "Static_Row," << rowCount   << ",Read Channel,Read Data Transfers " << std::endl;
      fout << "Static_Row," << ++rowCount << ",Write Channel,Write Data Transfers " << std::endl;
      fout << "Group_End," << portAndArgs << std::endl;
    }
  }

  void DeviceTraceWriter::writeCUStreamTransfersStructure(XclbinInfo* xclbin, ComputeUnitInstance* cu, uint32_t& rowCount)
  {
    // Generate Wave group for stream data transfers if enabled
    if (!(cu->streamEnabled())) return ;

    std::vector<uint32_t>* cuASMs = cu->getASMs() ;
    for (auto cuASM : *cuASMs) {
      Monitor* ASM = (db->getStaticInfo()).getASMonitor(deviceId, xclbin, cuASM) ;
      if (nullptr == ASM) continue ;

      std::pair<XclbinInfo*, uint32_t> index = std::make_pair(xclbin, cuASM) ;
      asmBucketIdMap[index] = ++rowCount ;

      // KERNEL_STREAM_READ/WRITE
      fout << "Group_Start," << ASM->name << ",AXI Stream transaction over " << ASM->name << std::endl;
      fout << "Static_Row," << rowCount << ",Stream Activity,AXI Stream transactions over " << ASM->name << std::endl;
      fout << "Static_Row," << ++rowCount << ",Link Stall" << std::endl;
      fout << "Static_Row," << ++rowCount << ",Link Starve" << std::endl;
      fout << "Group_End," << ASM->name << std::endl;
    }
  }

  void DeviceTraceWriter::writeFloatingMemoryTransfersStructure(XclbinInfo* xclbin, uint32_t& rowCount)
  {
    if (!(db->getStaticInfo().hasFloatingAIM(deviceId, xclbin))) return ;
    fout << "Group_Start,AXI Memory Monitors,Read/Write data transfers over AXI Memory Mapped connection " << std::endl;

    // Go through all of the AIMs in this xclbin to find the floating ones
    std::map<uint64_t, Monitor*> *aimMap =
      (db->getStaticInfo()).getAIMonitors(deviceId, xclbin);
    
    size_t i = 0;
    for(auto& entry : *aimMap) {
      Monitor* aim = entry.second;
      if(nullptr == aim) {
        continue;
      }
      if(-1 != aim->cuIndex) {
        // not a floating AIM, must have been covered in CU section
        i++;
        continue;
      }

      std::pair<XclbinInfo*, uint32_t> index = std::make_pair(xclbin, static_cast<uint32_t>(i)) ;
      aimBucketIdMap[index] = ++rowCount;

      std::string portAndArgs = aim->name;
      if(!aim->args.empty()) {
        portAndArgs += " (" + aim->args + ")";
      }
      fout << "Group_Start," << portAndArgs << ",Data Transfers over read and write channels of AXI Memory Mapped " << aim->name << std::endl;
      fout << "Static_Row,"  << rowCount   << ",Read Channel,Read Data Transfers " << std::endl;
      fout << "Static_Row,"  << ++rowCount << ",Write Channel,Write Data Transfers " << std::endl;
      fout << "Group_End,"   << portAndArgs << std::endl ;
      i++;
    }
    fout << "Group_End,AXI Memory Monitors" << std::endl ;
  }

  void DeviceTraceWriter::writeFloatingStreamTransfersStructure(XclbinInfo* xclbin, uint32_t& rowCount)
  {
    if (!(db->getStaticInfo()).hasFloatingASM(deviceId, xclbin)) return ;
    fout << "Group_Start,AXI Stream Monitors,Data transfers over AXI Stream connection " << std::endl;

    std::map<uint64_t, Monitor*> *asmMap =
      (db->getStaticInfo()).getASMonitors(deviceId, xclbin);
    size_t i = 0 ;
    for(auto& entry : *asmMap) {
      Monitor* asM = entry.second;
      if(nullptr == asM) {
        continue;
      }
      if(-1 != asM->cuIndex) {
        // not a floating ASM, must have been covered in CU section
        i++;
        continue;
      }

      std::pair<XclbinInfo*, uint32_t> index = std::make_pair(xclbin, static_cast<uint32_t>(i)) ;
      asmBucketIdMap[index] = ++rowCount;
      fout << "Group_Start," << asM->name << ",AXI Stream transactions over " << asM->name << std::endl;
      fout << "Static_Row," << rowCount << ",Stream Activity,AXI Stream transactions over " << asM->name << std::endl;
      fout << "Static_Row," << ++rowCount << ",Link Stall" << std::endl;
      fout << "Static_Row," << ++rowCount << ",Link Starve" << std::endl;
      fout << "Group_End," << asM->name << std::endl;
      i++;
    }
    fout << "Group_End,AXI Stream Monitors" << std::endl ;
  }

  void DeviceTraceWriter::writeStructure()
  {
    fout << "STRUCTURE" << std::endl ;
    
    // Use the database's "static" information to discover how many
    //  kernels, compute units, etc. this device has.  Then, use that
    //  to build up the structure of the file we are generating
    
    std::string deviceName = (db->getStaticInfo()).getDeviceName(deviceId) ;
    fout << "Group_Start," << deviceName << std::endl ;
    writeDeviceStructure() ;
    writeLoadedXclbinsStructure() ;
    fout << "Group_End," << deviceName << std::endl ;
  }

  void DeviceTraceWriter::writeStringTable()
  {
    fout << "MAPPING" << std::endl ;
    (db->getDynamicInfo()).dumpStringTable(fout) ;
  }

  void DeviceTraceWriter::writeTraceEvents()
  {
    fout << "EVENTS" << std::endl;
    auto DeviceEvents = (db->getDynamicInfo()).getEraseDeviceEvents(deviceId);

    std::vector<XclbinInfo*> loadedXclbins =
      (db->getStaticInfo()).getLoadedXclbins(deviceId) ;
    if (loadedXclbins.size() <= 0) {
      return ;
    }
    int xclbinIndex = 0 ;
    XclbinInfo* xclbin = loadedXclbins[xclbinIndex] ;

    for(auto& e : DeviceEvents) {
      VTFDeviceEvent* deviceEvent = dynamic_cast<VTFDeviceEvent*>(e.get());
      if(!deviceEvent)
        continue;
      
      int32_t cuId = deviceEvent->getCUId();
      VTFEventType eventType = deviceEvent->getEventType();
      if (XCLBIN_END == eventType) {
        // If we hit the end of an xclbin's execution, then increment xclbins
        xclbin = loadedXclbins[++xclbinIndex] ;
      } else if (KERNEL == eventType) {
        KernelEvent* kernelEvent = dynamic_cast<KernelEvent*>(deviceEvent) ;
        if (kernelEvent == nullptr) continue ; // Coverity - In case dynamic cast fails
        std::pair<XclbinInfo*, int32_t> index =
          std::make_pair(xclbin, cuId) ;
        kernelEvent->dump(fout, cuBucketIdMap[index] + eventType - KERNEL) ;
        // Also output the tool tips
        for (auto iter : xclbin->cus) {
          ComputeUnitInstance* cu = iter.second ;
          if (cu->getAccelMon() == cuId) {
            fout << "," << db->getDynamicInfo().addString(cu->getKernelName());
            fout << "," << db->getDynamicInfo().addString(cu->getName());
          }
        }
        fout << std::endl ;
      } else if(KERNEL_STALL_EXT_MEM == eventType
                || KERNEL_STALL_DATAFLOW == eventType
                || KERNEL_STALL_PIPE == eventType) {
        std::pair<XclbinInfo*, int32_t> index =
          std::make_pair(xclbin, cuId) ;
        deviceEvent->dump(fout, cuBucketIdMap[index] + eventType - KERNEL);
      } else {
        // Memory or Stream Acceses
        uint32_t monId = deviceEvent->getMonitorId();
        DeviceMemoryAccess* memoryEvent = dynamic_cast<DeviceMemoryAccess*>(e.get());
        if(memoryEvent) {
          std::pair<XclbinInfo*, uint32_t> index =std::make_pair(xclbin, monId);
          deviceEvent->dump(fout, aimBucketIdMap[index] + eventType - KERNEL_READ);
          continue;
        }
        DeviceStreamAccess* streamEvent = dynamic_cast<DeviceStreamAccess*>(e.get());
        if(streamEvent) {
          std::pair<XclbinInfo*, uint32_t> index = std::make_pair(xclbin, monId) ;
          if(KERNEL_STREAM_READ == eventType || KERNEL_STREAM_READ_STALL == eventType
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
    fout << "DEPENDENCIES" << std::endl ;
    // No dependencies in device events
  }

  bool DeviceTraceWriter::traceEventsExist()
  {
    return (db->getDynamicInfo()).deviceEventsExist(deviceId);
  }

  bool DeviceTraceWriter::write(bool openNewFile)
  {
    if (openNewFile && !traceEventsExist()) {
      return false;
    }

    initialize() ;

    writeHeader() ;
    fout << std::endl ;
    writeStructure() ;
    fout << std::endl ;
    writeStringTable() ;
    fout << std::endl ;
    writeTraceEvents() ;
    fout << std::endl ;
    writeDependencies() ;
    fout << std::endl ;

    if (openNewFile) switchFiles() ;
    return true;
  }

  void DeviceTraceWriter::initialize()
  {
    std::vector<XclbinInfo*> loadedXclbins =
      (db->getStaticInfo()).getLoadedXclbins(deviceId) ;

    for (auto xclbin : loadedXclbins) {
      for (auto iter : xclbin->cus) {
        ComputeUnitInstance* cu = iter.second ;
        db->getDynamicInfo().addString(cu->getKernelName()) ;
        db->getDynamicInfo().addString(cu->getName()) ;
      }
    }
  }

} // end namespace xdp
