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
#include "xdp/profile/database/database.h"

namespace xdp {

  HALHostTraceWriter::HALHostTraceWriter(const char* filename, 
					 const std::string& version,
					 const std::string& creationTime,
					 const std::string& xrtV) :
    VPTraceWriter(filename, version, creationTime, 6 /* us */),
    XRTVersion(xrtV)
  {
  }

  HALHostTraceWriter::~HALHostTraceWriter()
  {
  }

  void HALHostTraceWriter::writeHeader()
  {
    VPTraceWriter::writeHeader() ;
    fout << "XRT Version," << XRTVersion << std::endl ;
  }

  void HALHostTraceWriter::writeStructure()
  {
    // This section describes the hierarchy and structure of the wcfg
    //  and where each type of event we generate should end up.  It is
    //  based upon the static structure of the loaded xclbin in the
    //  device.
    fout << "STRUCTURE" << std::endl ;
    fout << "Group_Start,Host" << std::endl ;
    fout << "Group_Start,HAL API Calls" << std::endl ;
    fout << "Dynamic_General,General,0x0,HAL_API_CALL" << std::endl ;
    fout << "Group_End,HAL API Calls" << std::endl ;
    fout << "Group_Start,Data Transfer" << std::endl ;
    fout << "Dynamic_without_summary,Read, ,READ_BUFFER" << std::endl ;
    fout << "Dynamic_without_summary,Write, ,WRITE_BUFFER" << std::endl ;
    fout << "Dynamic_with_summary,Kernel Enqueues, ,KERNEL_ENQUEUE" 
	 << std::endl ;
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
					     return e->isHALAPI() ;
					   }
					 ) ;
    for (auto e : HALAPIEvents)
    {
      e->dump(fout, 0) ;
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
