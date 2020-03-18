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

#include "xdp/profile/writer/hal/hal_host_trace_writer.h"

namespace xdp {

  HALHostTraceWriter::HALHostTraceWriter(const char* filename, 
					 const std::string& version,
					 const std::string& creationTime,
					 const std::string& xrtV,
					 const std::string& toolV)
    : VPTraceWriter(filename, version, creationTime, 6 /* us */),
      xrtVersion(xrtV),
      toolVersion(toolV)
  {
  }

  HALHostTraceWriter::~HALHostTraceWriter()
  {
  }

  void HALHostTraceWriter::writeHeader()
  {
    VPTraceWriter::writeHeader() ;
    fout << "XRT  Version," << xrtVersion  << std::endl
         << "Tool Version," << toolVersion << std::endl;

    //fout << "Profiled Application," << xdp::WriterI::getCurrentExecutableName() << std::endl; // check
  }

  void HALHostTraceWriter::writeStructure()
  {
    // This section describes the hierarchy and structure of the wcfg
    //  and where each type of event we generate should end up.  It is
    //  based upon the static structure of the loaded xclbin in the
    //  device.
    uint32_t rowCount = 0;
    fout << "STRUCTURE" << std::endl ;
    
    fout << "Group_Start,Host" << std::endl ;

    fout << "Group_Start,HAL API Calls" << std::endl ;
    fout << "Dynamic_Row," << ++rowCount << ",General,0x0,API_CALL" << std::endl;
    eventTypeBucketIdMap[HAL_API_CALL] = rowCount;
    fout << "Group_End,HAL API Calls" << std::endl ;
    
    fout << "Group_Start,Data Transfer" << std::endl ;
    fout << "Dynamic_Row," << ++rowCount << ",Read,READ_BUFFER" << std::endl ;
    eventTypeBucketIdMap[READ_BUFFER] = rowCount;
    fout << "Dynamic_Row," << ++rowCount << ",Write,WRITE_BUFFER" << std::endl ;
    eventTypeBucketIdMap[WRITE_BUFFER] = rowCount;
    fout << "Group_End,Data Transfer" << std::endl ;
    
    fout << "Group_End,Host" << std::endl ;
  }

  void HALHostTraceWriter::writeStringTable()
  {
    fout << "MAPPING" << std::endl ;
    (db->getDynamicInfo()).dumpStringTable(fout) ;
  }

  void HALHostTraceWriter::writeTraceEvents()
  {
    fout << "EVENTS" << std::endl ;
    std::vector<VTFEvent*> HALAPIEvents = 
      (db->getDynamicInfo()).filterEvents( [](VTFEvent* e)
					   {
//               return e->isHALAPI() ;
					     return e->isHostEvent() ;
					   }
					 ) ;
    for (auto e : HALAPIEvents)
    {
      VTFEventType eventType = e->getEventType();
      e->dump(fout, eventTypeBucketIdMap[eventType]) ;
    }
  }

  void HALHostTraceWriter::writeDependencies()
  {
    fout << "DEPENDENCIES" << std::endl ;
    // No dependencies in HAL events
  }

  void HALHostTraceWriter::write(bool openNewFile)
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

}
