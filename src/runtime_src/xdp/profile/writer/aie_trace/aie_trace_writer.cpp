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

  // 10 Megabytes or 2.5M words
  constexpr uint64_t LARGE_DATA_WARN_THRESHOLD = 0xA00000;
  bool AIETraceWriter::largeDataWarning = false;

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
    try {
      if (fout.is_open()) {
        if (fout.tellp() <= 0) {
          std::string msg = "File: " + getcurrentFileName() + " (device #" + std::to_string(deviceId) 
              + ", stream #" + std::to_string(traceStreamId) + ") trace data was not captured.";
          xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
        }
        fout << std::endl;
      }
    } catch (...){
      std::string msg = "Trace File: " + getcurrentFileName() + " not found.";
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
      return;
    }

    size_t num = traceData->buffer.size();
    if (num == 0) {
      delete traceData;
      return;
    }

    if (!largeDataWarning) {
      uint64_t traceBytes = 0;
      for (size_t j = 0; j < num; j++)
        traceBytes += traceData->bufferSz[j];
      if (traceBytes > LARGE_DATA_WARN_THRESHOLD) {
        std::string msg = "Writing large amount of AIE trace. This could take a while.";
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
        largeDataWarning = true;
      }
    }

    for (size_t j = 0; j < num; j++) {
      void*    buf = traceData->buffer[j];
      if (nullptr == buf)
        continue;

      // We write 4 bytes at a time
      // Max chunk size should be multiple of 4
      // If last chunk is not multiple of 4 then in worst case, 
      // 3 bytes of data will not be written. But this is not possible, as we always write full packet.
      uint64_t bufferSz = (traceData->bufferSz[j] / 4);

      uint32_t* dataBuffer = static_cast<uint32_t*>(buf);
      for (uint64_t i = 0; i < bufferSz; i++)
        fout << "0x" << std::hex << dataBuffer[i] << std::endl;

      // Free the memory immediately if we own it
      if (traceData->owner)
        delete[] (traceData->buffer[j]);
    }
    delete traceData;
  }

  void AIETraceWriter::writeDependencies()
  {
  }

  bool AIETraceWriter::write(bool /*openNewFile*/)
  {
    writeTraceEvents();
    return true;
  }

}
