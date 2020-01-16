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
					     const std::string& version,
					     const std::string& creationTime,
					     const std::string& xrtV,
					     DeviceIntf* d) :
    VPTraceWriter(filename, version, creationTime, 9 /* ns */),
    XRTVersion(xrtV),
    dev(d)
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
    fout << "STRUCTURE" << std::endl ;
    
    // Use the database's "static" information to discover how many
    //  kernels, compute units, etc. this device has.  Then, use that
    //  to build up the structure of the file we are generating
    
    std::string deviceName = (db->getStaticInfo()).getDeviceName(dev) ;
    std::string xclbinName = (db->getStaticInfo()).getXclbinName(dev) ;

    fout << "Group_start," << deviceName << std::endl ;
    fout << "Group_start," << xclbinName << std::endl ;

    uint16_t numKDMA = (db->getStaticInfo()).getKDMACount(dev) ;

    if (numKDMA > 0)
    {
      fout << "Group_start,KDMA" << std::endl ;
      for (unsigned int i = 0 ; i < numKDMA ; ++i)
      {
	fout << "Dynamic_without_summary,Read, ,KERNEL_READ" << std::endl ;
	fout << "Dynamic_without_summary,Write, ,KERNEL_WRITE" << std::endl ;
      }
      fout << "Group_end,KDMA" << std::endl ;
    }

    std::vector<ComputeUnitInstance> cus = (db->getStaticInfo()).getCUs(dev) ;
    for (unsigned int i = 0 ; i < cus.size() ; ++i)
    {
      fout << "Group_start," << cus[i].getName() << std::endl ;
      fout << "Dynamic_with_summary,Executions, ,KERNEL" << std::endl ;
      // For each memory bank/destination that could be accessed by this
      //  compute unit, create a row.

      fout << "Group_end," << cus[i].getName() << std::endl ;
    }

    fout << "Group_end," << xclbinName << std::endl ;
    fout << "Group_end," << deviceName << std::endl ;

  }

  void HALDeviceTraceWriter::writeStringTable()
  {
    fout << "MAPPING" << std::endl ;
    (db->getDynamicInfo()).dumpStringTable(fout) ;
  }

  void HALDeviceTraceWriter::writeTraceEvents()
  {
    fout << "EVENTS" << std::endl ;
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
