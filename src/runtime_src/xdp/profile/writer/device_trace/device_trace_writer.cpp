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
    fout << "XRT  Version," << xrtVersion  << std::endl
         << "Tool Version," << toolVersion << std::endl
         << "Platform," << (db->getStaticInfo()).getDeviceName(deviceId) << std::endl
         << "Target,System Run" << std::endl;    // hardcoded for now
  }

  void DeviceTraceWriter::writeStructure()
  {
    uint32_t rowCount = 0;
    fout << "STRUCTURE" << std::endl ;
    
    // Use the database's "static" information to discover how many
    //  kernels, compute units, etc. this device has.  Then, use that
    //  to build up the structure of the file we are generating
    
    std::string deviceName = (db->getStaticInfo()).getDeviceName(deviceId) ;
    std::string xclbinName = (db->getStaticInfo()).getXclbinName(deviceId) ;

    fout << "Group_Start," << deviceName << std::endl ;
    fout << "Group_Start," << xclbinName << std::endl ;

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

    std::map<int32_t, ComputeUnitInstance*> *cus = (db->getStaticInfo()).getCUs(deviceId);
    if(cus) {
      for(auto itr : *cus) {
        ComputeUnitInstance* cu = itr.second;
        std::string cuName = cu->getName();

        // Wave Group for CU
        fout << "Group_Start,Compute Unit " << cuName << ",Activity in accelerator "<< cu->getKernelName() << ":" << cuName << std::endl ;
        fout << "Dynamic_Row_Summary," << ++rowCount << ",Executions,Execution in accelerator " << cuName << std::endl;
        cuBucketIdMap[cu->getIndex()] = rowCount;

        // Wave Group for Kernel Stall, if Stall monitoring is enabled in CU
        if(cu->stallEnabled()) {
          // KERNEL_STALL : stall type
          fout << "Group_Summary_Start,Stall,Stalls in accelerator " << cuName << std::endl;
          fout << "Static_Row," << (rowCount + KERNEL_STALL_EXT_MEM - KERNEL)  << ",External Memory Stall, Stalls from accessing external memory" << std::endl;
          fout << "Static_Row," << (rowCount + KERNEL_STALL_DATAFLOW - KERNEL) << ",Intra-Kernel Dataflow Stall,Stalls from dataflow streams inside compute unit" << std::endl;
          fout << "Static_Row," << (rowCount + KERNEL_STALL_PIPE - KERNEL) << ",Inter-Kernel Pipe Stall,Stalls from accessing pipes between kernels" << std::endl;
          fout << "Group_End,Stall" << std::endl;

          rowCount += (KERNEL_STALL_PIPE - KERNEL);
        }

        // Wave Group for Read and Write, if Data transfer monitoring is enabled in CU
        if(cu->dataTransferEnabled()) {
          std::vector<uint32_t> *cuAIMs  = cu->getAIMs();
          for(auto cuAIM : *cuAIMs) {
            Monitor* aim = (db->getStaticInfo()).getAIMonitor(deviceId, cuAIM);
            if(nullptr == aim) {
              continue;
            }
            ++rowCount;
            aimBucketIdMap[cuAIM] = rowCount;

            // Read : KERNEL_READ
            fout << "Group_Start,Read,Read data transfers between " << cuName << " and Global Memory over " << aim->name << std::endl;
            fout << "Static_Row," << rowCount << "," << aim->name << ",Read Data Transfers " << std::endl;
            fout << "Group_End,Read" << std::endl;

  
            // Write : KERNEL_WRITE
            fout << "Group_Start,Write,Write data transfers between " << cuName << " and Global Memory over " << aim->name << std::endl;
            fout << "Static_Row," << ++rowCount << "," << aim->name << ",Write Data Transfers " << std::endl;
            fout << "Group_End,Write" << std::endl;
          }
        }

        if(cu->streamEnabled()) {
          std::vector<uint32_t> *cuASMs  = cu->getASMs();
          for(auto cuASM : *cuASMs) {
            Monitor* asM = (db->getStaticInfo()).getASMonitor(deviceId, cuASM);
            if(nullptr == asM) {
              continue;
            }
            ++rowCount;
            asmBucketIdMap[cuASM] = rowCount;

            // KERNEL_STREAM_READ/WRITE
            fout << "Group_Start,Stream Transfers,AXI Stream transaction over " << asM->name << std::endl;
            fout << "Static_Row," << rowCount << "," << asM->name << ",AXI Stream transactions over " << asM->name << std::endl;
            fout << "Static_Row," << ++rowCount << ",Link Stall" << std::endl;
            fout << "Static_Row," << ++rowCount << ",Link Starve" << std::endl;
            fout << "Group_End,Stream Transfers" << std::endl;
          }
        }
        fout << "Group_End," << cuName << std::endl ;
// HOST READ?WRITE
      }
    }

    if((db->getStaticInfo()).hasFloatingAIM(deviceId)) {
      fout << "Group_Start,AXI Memory Monitors,Read/Write data transfers over AXI Memory Mapped connection " << std::endl;
      std::map<uint64_t, Monitor*> *aimMap = (db->getStaticInfo()).getAIMonitors(deviceId);
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
#if 0
        // This check is not required anymore as "getAIMonitors" returns user space AIM only but no shell AIM
        // If monitor name starts with "shell", then it is a shell monitor and trace is not available. So, skip it.
        size_t pos = aim->name.find('/');
        if(0 == aim->name.substr(0, pos).compare("shell")) {
          continue;
        }
#endif
        aimBucketIdMap[i] = ++rowCount;
        fout << "Group_Start," << aim->name  << " AXI Memory Monitor,Read/Write data transfers over AXI Memory Mapped " << aim->name << std::endl;
        fout << "Static_Row,"  << rowCount   << ",Read transfers,Read transfers for "  << aim->name << std::endl;
        fout << "Static_Row,"  << ++rowCount << ",Write transfers,Write transfers for " << aim->name << std::endl;
        fout << "Group_End,"   << aim->name  << " AXI Memory Monitor" << std::endl ;
        i++;
      }
      fout << "Group_End,AXI Memory Monitors" << std::endl ;
    }

    if((db->getStaticInfo()).hasFloatingASM(deviceId)) {
      fout << "Group_Start,AXI Stream Monitors,Data transfers over AXI Stream connection " << std::endl;
      std::map<uint64_t, Monitor*> *asmMap = (db->getStaticInfo()).getASMonitors(deviceId);
      size_t i = 0;
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
#if 0
        // This check is not required anymore as "getASMonitors" returns user space ASM only but no shell ASM
        // If monitor name starts with "shell", then it is a shell monitor and trace is not available. So, skip it.
        size_t pos = asM->name.find('/');
        if(0 == asM->name.substr(0, pos).compare("shell")) {
          continue;
        }
#endif
        asmBucketIdMap[i] = ++rowCount;
        fout << "Group_Start," << asM->name  << " AXI Stream Monitor,Read/Write data transfers over AXI Stream " << asM->name << std::endl;
        fout << "Static_Row,"  << rowCount   << ",Stream Port,AXI Stream Read/Write transaction over " << asM->name << std::endl;
        fout << "Static_Row,"  << ++rowCount << ",Link Stall,Stall during transaction over " << asM->name << std::endl;
        fout << "Static_Row,"  << ++rowCount << ",Link Starve,Starve during transaction over " << asM->name << std::endl;
        fout << "Group_End,"   << asM->name  << " AXI Stream Monitor" << std::endl;
        i++;
      }
      fout << "Group_End,AXI Stream Monitors" << std::endl ;
    }


    fout << "Group_End," << xclbinName << std::endl ;
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
    std::vector<VTFEvent*> DeviceEvents = (db->getDynamicInfo()).getDeviceEvents(deviceId);

    for(auto e : DeviceEvents) {
      VTFDeviceEvent* deviceEvent = dynamic_cast<VTFDeviceEvent*>(e);
      if(!deviceEvent)
        continue;
      int32_t cuId = deviceEvent->getCUId();
      VTFEventType eventType = deviceEvent->getEventType();
      if(KERNEL == eventType || KERNEL_STALL_EXT_MEM == eventType
                             || KERNEL_STALL_DATAFLOW == eventType
                             || KERNEL_STALL_PIPE == eventType) {
        deviceEvent->dump(fout, cuBucketIdMap[cuId] + eventType - KERNEL);
      } else {
        // Memory or Stream Acceses
        uint32_t monId = deviceEvent->getMonitorId();
        DeviceMemoryAccess* memoryEvent = dynamic_cast<DeviceMemoryAccess*>(e);
        if(memoryEvent) {
          deviceEvent->dump(fout, aimBucketIdMap[monId] + eventType - KERNEL_READ);
          continue;
        }
        DeviceStreamAccess* streamEvent = dynamic_cast<DeviceStreamAccess*>(e);
        if(streamEvent) {
          if(KERNEL_STREAM_READ == eventType || KERNEL_STREAM_READ_STALL == eventType
                                             || KERNEL_STREAM_READ_STARVE == eventType) {
            deviceEvent->dump(fout, asmBucketIdMap[monId] + eventType - KERNEL_STREAM_READ);
          } else {
            deviceEvent->dump(fout, asmBucketIdMap[monId] + eventType - KERNEL_STREAM_WRITE);
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

  void DeviceTraceWriter::write(bool openNewFile)
  {
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
  }

} // end namespace xdp
