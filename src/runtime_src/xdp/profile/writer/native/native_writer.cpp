/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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
#include "xdp/profile/database/events/native_events.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/native/native_writer.h"

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
    fout << "XRT Version," << getToolVersion() << "\n" ;
  }

  void NativeTraceWriter::writeStructure()
  {
    // There is only one bucket where all the APIs will go
    fout << "STRUCTURE" << "\n" ;
    fout << "Group_Start,Native API Host Trace\n" ;
    fout << "Dynamic_Row," << APIBucket << ",Native XRT API Calls,API Events" << "\n" ;
    fout << "Group_Start,Host to Device Data Transfers\n" ;
    fout << "Dynamic_Row," << readBucket << ",Reads,Read Transfers\n" ;
    fout << "Dynamic_Row," << writeBucket << ",Writes,Write Transfers\n" ;
    fout << "Group_End,Host to Device Data Transfers\n" ;
    fout << "Group_End,Native API Host Trace\n" ;
  }

  void NativeTraceWriter::writeStringTable()
  {
    fout << "MAPPING" << "\n" ;
    (db->getDynamicInfo()).dumpStringTable(fout) ;
  }

  void NativeTraceWriter::writeTraceEvents()
  {
    std::vector<VTFEvent*> APIEvents =
      (db->getDynamicInfo()).moveUnsortedHostEvents(
        [](VTFEvent* e)
        {
          return e->isNativeHostEvent();
        } ) ;

    std::sort(APIEvents.begin(), APIEvents.end(),
              [](VTFEvent* x, VTFEvent* y)
                {
                  if (x->getTimestamp() < y->getTimestamp()) return true;
                  return false;
                }) ;

    fout << "EVENTS" << "\n";
    for (auto& e : APIEvents) {
      // If this is a read/write, then dump the event in the other bucket
      if (e->isNativeRead())
        e->dumpSync(fout, readBucket);
      else if (e->isNativeWrite())
        e->dumpSync(fout, writeBucket);
      else
        e->dump(fout, APIBucket);
    }

    for (auto& e : APIEvents)
      delete e;
  }

  void NativeTraceWriter::writeDependencies()
  {
    fout << "DEPENDENCIES" << "\n" ;
    // No dependencies in Native XRT APIs
  }

  bool NativeTraceWriter::write(bool openNewFile)
  {
    writeHeader() ;       fout << "\n" ;
    writeStructure() ;    fout << "\n" ;
    writeStringTable() ;  fout << "\n" ;
    writeTraceEvents() ;  fout << "\n" ;
    writeDependencies() ; fout << std::endl ; // Force a flush at the end

    if (openNewFile) switchFiles() ;

    return true ;
  }

} // end namespace xdp
