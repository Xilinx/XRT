/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. = All rights reserved
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
    fout << "TraceID," << traceID << "\n"
         << "XRT  Version," << xrtVersion  << "\n"
         << "Tool Version," << toolVersion << "\n";

    //fout << "Profiled Application," << xdp::WriterI::getCurrentExecutableName() << "\n"; // check
  }

  void HALHostTraceWriter::writeStructure()
  {
    // This section describes the hierarchy and structure of the wcfg
    //  and where each type of event we generate should end up.  It is
    //  based upon the static structure of the loaded xclbin in the
    //  device.
    uint32_t rowCount = 0;
    fout << "STRUCTURE" << "\n" ;
    
    fout << "Group_Start,HAL Host Trace" << "\n" ;

    fout << "Dynamic_Row," << ++rowCount << ",HAL API Calls,API_CALL" << "\n";
    eventTypeBucketIdMap[HAL_API_CALL] = rowCount;
    
    fout << "Group_Start,Data Transfer" << "\n" ;
    fout << "Dynamic_Row," << ++rowCount << ",Read,READ_BUFFER" << "\n" ;
    eventTypeBucketIdMap[READ_BUFFER] = rowCount;
    fout << "Dynamic_Row," << ++rowCount << ",Write,WRITE_BUFFER" << "\n" ;
    eventTypeBucketIdMap[WRITE_BUFFER] = rowCount;
    fout << "Group_End,Data Transfer" << "\n" ;
    
    fout << "Group_End,HAL Host Trace" << "\n" ;
  }

  void HALHostTraceWriter::writeStringTable()
  {
    fout << "MAPPING" << "\n" ;
    (db->getDynamicInfo()).dumpStringTable(fout) ;
  }

  void HALHostTraceWriter::writeTraceEvents()
  {
    fout << "EVENTS\n";
    std::vector<VTFEvent*> HALAPIEvents = 
      db->getDynamicInfo().copySortedHostEvents( [](VTFEvent* e)
                                                 {
                                                   return e->isHostEvent()  &&
                                                          !e->isOpenCLAPI() &&
                                                          !e->isLOPHostEvent();
                                                 }
                                               );
    for (auto e : HALAPIEvents) {
      VTFEventType eventType = e->getEventType();
      e->dump(fout, eventTypeBucketIdMap[eventType]) ;
    }
  }

  void HALHostTraceWriter::writeDependencies()
  {
    fout << "DEPENDENCIES" << "\n" ;
    // No dependencies in HAL events
  }

  bool HALHostTraceWriter::write(bool openNewFile)
  {
    writeHeader() ;
    fout << "\n" ;
    writeStructure() ;
    fout << "\n" ;
    writeStringTable() ;
    fout << "\n" ;
    writeTraceEvents() ;
    fout << "\n" ;
    writeDependencies() ;
    fout << "\n" ;

    if (openNewFile) switchFiles() ;
    return true;
  }

}
