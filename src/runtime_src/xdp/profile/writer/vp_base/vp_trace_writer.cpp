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

#define XDP_SOURCE

#include "xdp/profile/writer/vp_base/vp_trace_writer.h"
#include "xdp/profile/database/database.h"
#include <iostream>

namespace xdp {

  std::atomic<unsigned int> VPTraceWriter::traceIDCtr{0};

  VPTraceWriter::VPTraceWriter(const char* filename,
				 const std::string& v,
				 const std::string& c,
				 uint16_t r) :
    VPWriter(filename),
    version(v), creationTime(c), resolution(r),
    humanReadable(true)
  {
    setUniqueTraceID();
  }

  VPTraceWriter::~VPTraceWriter()
  {
  }

  void VPTraceWriter::writeHeader()
  {
    fout << "HEADER" << std::endl
         << "VTF File Version," << version << std::endl ;
    fout << "VTF File Type," ;
    if      (isHost())   fout << "0" ;
    else if (isDevice()) fout << "1" ;
    else if (isAIE())    fout << "2" ;
    else if (isKernel()) fout << "3" ;
    fout << std::endl ;
    fout << "PID," << (db->getStaticInfo()).getPid() << std::endl
         << "Generated on," << creationTime << std::endl
         << "Resolution,ms" << std::endl
         << "Min Resolution," << (resolution == 6 ? "us" : "ns") << std::endl
         << "Trace Version," << version << std::endl; 
  }

  void VPTraceWriter::setUniqueTraceID()
  {
    unsigned int pid = static_cast<unsigned int>(db->getStaticInfo().getPid());
    traceID = pid + traceIDCtr++;
  }

}
