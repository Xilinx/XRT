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

#include "xdp/profile/writer/aie_trace/aie_trace_writer.h"

#include <iostream>

namespace xdp {

  AIETraceWriter::AIETraceWriter(const char* filename, uint64_t devId, uint64_t trStrmId,
                                 const std::string& version, 
                                 const std::string& creationTime, 
                                 const std::string& /*xrtV*/, 
                                 const std::string& /*toolV*/)
    : VPTraceWriter(filename, version, creationTime, 6 /* us */),
      deviceId(devId),
      traceStreamId(trStrmId)
#if 0
      xrtVersion(xrtV),
      toolVersion(toolV)
#endif
  {
  }

  AIETraceWriter::~AIETraceWriter()
  {
  }

  void AIETraceWriter::writeHeader()
  {
  }

  void AIETraceWriter::writeStructure()
  {
  }

  void AIETraceWriter::writeStringTable()
  {
  }

  void AIETraceWriter::writeTraceEvents()
  {
std::cout << " in AIETraceWriter::writeTraceEvents : about to call getAIETraceData : deviceId " << deviceId << " traceStreamId " << traceStreamId << std::endl;
    // write the entire buffer
    AIETraceDataType* traceData = (db->getDynamicInfo()).getAIETraceData(deviceId, traceStreamId);
    if(nullptr == traceData) {
std::cout << " in AIETraceWriter::writeTraceEvents : null traceData " << std::endl;
      fout << std::endl;
      return;
    }

std::cout << " in AIETraceWriter::writeTraceEvents : RIGHT AFTER getAIETraceData : deviceId " << deviceId << " traceStreamId " << traceStreamId << std::endl;
    void*    buf = traceData->buffer;
    uint64_t bufferSz = traceData->bufferSz;

std::cout << " in AIETraceWriter::writeTraceEvents : after getAIETraceData : deviceId " << deviceId << " traceStreamId " << traceStreamId << " buf " << buf << " bufferSz " << bufferSz << std::endl;
    if(nullptr == buf) {
std::cout << " in AIETraceWriter::writeTraceEvents : after getAIETraceData : deviceId " << deviceId << " traceStreamId " << traceStreamId << " buf " << buf << " EMPTY " << std::endl;
      fout << std::endl;
      return;
    }

    uint32_t* dataBuffer = static_cast<uint32_t*>(buf);
    for(uint64_t i = 0; i < bufferSz; i++) {
      fout << "0x" << std::hex << dataBuffer[i] << std::endl;
    }
std::cout << " in AIETraceWriter::writeTraceEvents : after WRITE : deviceId " << deviceId << " traceStreamId " << traceStreamId << " buf " << buf << " bufferSz " << bufferSz << std::endl;

    fout << std::endl;
  }

  void AIETraceWriter::writeDependencies()
  {
  }

  void AIETraceWriter::write(bool openNewFile)
  {
#if 0
    writeHeader() ;
    fout << std::endl ;
    writeStructure() ;
    fout << std::endl ;
    writeStringTable() ;
    fout << std::endl ;
#endif
    writeTraceEvents() ;
    fout << std::endl ;
#if 0
    writeDependencies() ;
    fout << std::endl ;
#endif

    if (openNewFile) switchFiles() ;
  }

}
