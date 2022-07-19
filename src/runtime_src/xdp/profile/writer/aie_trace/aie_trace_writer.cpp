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
#include "core/common/message.h"

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

    std::string dId = std::to_string(deviceId);
    std::string tId = std::to_string(traceStreamId);

    std::string filename = "aie_trace_" + dId + "_" + tId + ".txt";

    try {
      // Check if final file output is empty and throw a warning.
      std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);

      // \n is 2 bytes
      if (in.tellg() <= 2){
        std::string msg = "File: " + filename + " (device #" + dId + ", stream #" + tId + ") trace data was not captured.";
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      }

    } catch (...){
        std::string msg = "Trace File: " + filename + " not found.";
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
    }
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
    // write the entire buffer
    AIETraceDataType* traceData = (db->getDynamicInfo()).getAIETraceData(deviceId, traceStreamId);
    if (nullptr == traceData) {
      fout << std::endl;
      return;
    }

    size_t num = traceData->buffer.size();
    for (size_t j = 0; j < num; j++) {
      void*    buf = traceData->buffer[j];
      // We write 4 bytes at a time
      // Max chunk size should be multiple of 4
      // If last chunk is not multiple of 4 then in worst case, 
      // 3 bytes of data will not be written. But this is not possible, as we always write full packet.
      uint64_t bufferSz = (traceData->bufferSz[j] / 4);
      if (nullptr == buf) {
        fout << std::endl;
        return;
      }

      uint32_t* dataBuffer = static_cast<uint32_t*>(buf);
      for (uint64_t i = 0; i < bufferSz; i++)
        fout << "0x" << std::hex << dataBuffer[i] << std::endl;

      // Free the memory immediately if we own it
      if (traceData->owner)
        delete[] (traceData->buffer[j]);
    }
    fout << std::endl;
    delete traceData;
  }

  void AIETraceWriter::writeDependencies()
  {
  }

  bool AIETraceWriter::write(bool /*openNewFile*/)
  {
    writeTraceEvents();
    fout << std::endl;
    return true;
  }

}
