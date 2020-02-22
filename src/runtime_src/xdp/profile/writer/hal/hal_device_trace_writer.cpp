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

#include "xdp/profile/writer/hal/hal_device_trace_writer.h"
#include "xdp/profile/database/database.h"

namespace xdp {

  HALDeviceTraceWriter::HALDeviceTraceWriter(const char* filename,
               uint64_t devId, 
					     const std::string& version,
					     const std::string& creationTime,
					     const std::string& xrtV,
					     DeviceIntf* d) :
    VPTraceWriter(filename, version, creationTime, 9 /* ns */),
    XRTVersion(xrtV),
    dev(d),
    deviceId(devId)
  {
  }

  HALDeviceTraceWriter::~HALDeviceTraceWriter()
  {
    if (dev != nullptr) delete dev ;
  }

  void HALDeviceTraceWriter::writeHeader()
  {
    VPTraceWriter::writeHeader() ;
    fout << "XRT Version," << XRTVersion << std::endl ;
  }

  void HALDeviceTraceWriter::writeStructure()
  {
    uint32_t rowCount = 0;
    fout << "STRUCTURE" << std::endl ;
    
    // Use the database's "static" information to discover how many
    //  kernels, compute units, etc. this device has.  Then, use that
    //  to build up the structure of the file we are generating
    
    std::string deviceName = (db->getStaticInfo()).getDeviceName(deviceId) ;
//    std::string xclbinName = (db->getStaticInfo()).getXclbinUUID(deviceId) ;
    std::string xclbinName = "xclbin";
    
    fout << "Group_Start," << deviceName << std::endl ;
    fout << "Group_Start," << xclbinName << std::endl ;

    uint16_t numKDMA = (db->getStaticInfo()).getKDMACount(deviceId) ;
    if(numKDMA) {
      fout << "Group_Start,KDMA" << std::endl ;
      for (unsigned int i = 0 ; i < numKDMA ; ++i)
      {
	      fout << "Dynamic_Row," << ++rowCount << ",Read, ,KERNEL_READ" << std::endl;
	      fout << "Dynamic_Row," << ++rowCount << ",Write, ,KERNEL_WRITE" << std::endl;
      }
      fout << "Group_End,KDMA" << std::endl ;
    }

    std::map<int32_t, ComputeUnitInstance*> *cus = (db->getStaticInfo()).getCUs(deviceId);
//    std::map<int32_t, Memory*>           *memory = (db->getStaticInfo()).getMemoryInfo(deviceId);
    if(cus) {
      for(auto itr : *cus) {
        ComputeUnitInstance* cu = itr.second;
        fout << "Group_Start," << cu->getName() << std::endl ;
        fout << "Dynamic_Row_Summary," << ++rowCount << ",Executions, ,KERNEL" << std::endl;
        eventTypeBucketIdMap[KERNEL] = rowCount;
        fout << "Dynamic_Row_Summary," << ++rowCount << ",Read, ,KERNEL_READ" << std::endl ;
        eventTypeBucketIdMap[KERNEL_READ] = rowCount;
        fout << "Dynamic_Row_Summary," << ++rowCount << ",Write, ,KERNEL_WRITE" << std::endl ;
        eventTypeBucketIdMap[KERNEL_WRITE] = rowCount;
#if 0
        // For each memory bank/destination that could be accessed by this compute unit, create a row.
        std::map<int32_t, std::vector<int32_t>> *args = cu->getConnections();
        if(memory && args) {
          int32_t t = 0;
          for(auto itr : *args) {
            int32_t argIdx = itr.first;
            int32_t memIdx = (itr.second)[t]; // for now just one
            std::string argStr = "arg" + argIdx;
			fout << "Group_Start," << argStr << std::endl;
            fout << "Dynamic_Row," << ++rowCount << ",ArgMemory" << memIdx << ", ,KERNEL_READ" << std::endl;
            ++t;
          }
        }
#endif
        fout << "Group_End," << cu->getName() << std::endl ;
      }
    }

    fout << "Group_End," << xclbinName << std::endl ;
    fout << "Group_End," << deviceName << std::endl ;

  }

  void HALDeviceTraceWriter::writeStringTable()
  {
    fout << "MAPPING" << std::endl ;
    (db->getDynamicInfo()).dumpStringTable(fout) ;
  }

  void HALDeviceTraceWriter::writeTraceEvents()
  {
    fout << "EVENTS" << std::endl;
    std::vector<VTFEvent*> DeviceEvents = 
      (db->getDynamicInfo()).filterEvents( [](VTFEvent* e)
          { return e->isDeviceEvent(); });
    for(auto e : DeviceEvents) {
      VTFEventType eventType = e->getEventType();
      e->dump(fout, eventTypeBucketIdMap[eventType]);
    }
  }

  void HALDeviceTraceWriter::writeDependencies()
  {
    fout << "DEPENDENCIES" << std::endl ;
    // No dependencies in device events
  }

  void HALDeviceTraceWriter::write(bool openNewFile)
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
