/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#define XDP_SOURCE

#include "xdp/profile/writer/native/native_writer.h"
#include "xdp/profile/database/database.h"

#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

  NativeTraceWriter::NativeTraceWriter(const char* filename) :
    VPTraceWriter(filename, "1.0", getCurrentDateTime(), 9 /* ns */)
  {
  }

  NativeTraceWriter::~NativeTraceWriter()
  {
  }

  void NativeTraceWriter::writeHeader()
  {
    VPTraceWriter::writeHeader() ;
    fout << "XRT Version," << getToolVersion() << std::endl ;
  }

  void NativeTraceWriter::writeStructure()
  {
    // There is only one bucket where all the APIs will go
    fout << "STRUCTURE" << std::endl ;
    fout << "Group_Start,Host APIs" << std::endl ;
    fout << "Group_Start,Native XRT API Calls" << std::endl ;
    fout << "Dynamic_Row," << 1 << ",General,API Events" << std::endl ;
    fout << "Group_End,Native XRT API Calls" << std::endl ;
    fout << "Group_End,Host APIs" << std::endl ;
  }

  void NativeTraceWriter::writeStringTable()
  {
    fout << "MAPPING" << std::endl ;
    (db->getDynamicInfo()).dumpStringTable(fout) ;
  }

  void NativeTraceWriter::writeTraceEvents()
  {
    std::vector<std::unique_ptr<VTFEvent>> APIEvents = 
      (db->getDynamicInfo()).filterEraseHostEvents([](VTFEvent* e)
                                                   {
                                                     return e->isNativeHostEvent() ;
                                                   }
                                                  ) ;
    fout << "EVENTS" << std::endl ;
    for (auto& e : APIEvents) {
      e->dump(fout, 1) ; // 1 is the only bucket
    }
  }

  void NativeTraceWriter::writeDependencies()
  {
    fout << "DEPENDENCIES" << std::endl ;
    // No dependencies in Native XRT APIs
  }

  bool NativeTraceWriter::write(bool openNewFile)
  {
    writeHeader() ;       fout << std::endl ;
    writeStructure() ;    fout << std::endl ;
    writeStringTable() ;  fout << std::endl ;
    writeTraceEvents() ;  fout << std::endl ;
    writeDependencies() ; fout << std::endl ;

    if (openNewFile) switchFiles() ;

    return true ;
  }

} // end namespace xdp
