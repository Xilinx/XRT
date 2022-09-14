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

#ifndef AIE_TRACE_WRITER_H
#define AIE_TRACE_WRITER_H

#include <string>

#include "xdp/profile/writer/vp_base/vp_trace_writer.h"
#include "xdp/profile/database/database.h"

namespace xdp {

  // This is for AIE Trace 
  class AIETraceWriter : public VPTraceWriter
  {
  private:
    AIETraceWriter() = delete ;

    static bool largeDataWarning;

#if 0
    // Header information 
    std::string xrtVersion;
    std::string toolVersion;
#endif

   uint64_t deviceId;
   uint64_t traceStreamId;

  protected:
    virtual void writeHeader();
    virtual void writeStructure();
    virtual void writeStringTable();
    virtual void writeTraceEvents();
    virtual void writeDependencies();

  public:
    AIETraceWriter(const char* filename, uint64_t devId, uint64_t trStrmId,
                   const std::string& version, 
		   const std::string& creationTime, 
		   const std::string& xrtV,
		   const std::string& toolV);
    ~AIETraceWriter();

    virtual bool write(bool openNewFile);
  };

}

#endif
