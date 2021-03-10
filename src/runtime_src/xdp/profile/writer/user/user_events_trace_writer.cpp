/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include <vector>

#include "xdp/profile/writer/user/user_events_trace_writer.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/user_events.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

  UserEventsTraceWriter::UserEventsTraceWriter(const char* filename) :
    VPTraceWriter(filename,
		  "1.1",
		  getCurrentDateTime(),
		  9 /* ns */),
    bucketId(1)
  {
  }

  UserEventsTraceWriter::~UserEventsTraceWriter()
  {
  }

  void UserEventsTraceWriter::writeHeader()
  {
    VPTraceWriter::writeHeader() ;
    fout << "TraceID," << traceID << std::endl;
  }

  void UserEventsTraceWriter::writeStructure()
  {
    fout << "STRUCTURE" << std::endl ;
    fout << "Group_Start,User Events" << std::endl ;
    fout << "Dynamic_Row," << bucketId << ",General,User Events from APIs"
	 << std::endl ;
    fout << "Group_End,User Events" << std::endl ;
  }

  void UserEventsTraceWriter::writeStringTable()
  {
    fout << "MAPPING" << std::endl ;
    (db->getDynamicInfo()).dumpStringTable(fout) ;
  }

  void UserEventsTraceWriter::writeTraceEvents()
  {
    fout << "EVENTS" << std::endl ;
    std::vector<VTFEvent*> userEvents = 
      (db->getDynamicInfo()).filterEvents( [](VTFEvent* e)
					   {
					     return e->isUserEvent() ;
					   }
					 ) ;
    for (auto e : userEvents)
    {
      e->dump(fout, bucketId) ;
    }
  }

  void UserEventsTraceWriter::writeDependencies()
  {
    fout << "DEPENDENCIES" << std::endl ;
    // No dependencies in user events
  }

  bool UserEventsTraceWriter::write(bool openNewFile)
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

    if (openNewFile) switchFiles() ;

    return true;
  }

} // end namespace xdp
