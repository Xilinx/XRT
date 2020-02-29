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

#ifndef HAL_HOST_TRACE_WRITER_DOT_H
#define HAL_HOST_TRACE_WRITER_DOT_H

#include <string>

#include "xdp/profile/writer/vp_base/vp_trace_writer.h"

namespace xdp {

  // This is for host HAL APIs
  class HALHostTraceWriter : public VPTraceWriter
  {
  private:
    HALHostTraceWriter() = delete ;

    // Header information 
    std::string XRTVersion ;

  protected:
    virtual void writeHeader() ;
    virtual void writeStructure() ;
    virtual void writeStringTable() ;
    virtual void writeTraceEvents() ;
    virtual void writeDependencies() ;

  public:
    HALHostTraceWriter(const char* filename, const std::string& version, 
		       const std::string& creationTime, 
		       const std::string& xrtV) ;
    ~HALHostTraceWriter() ;

    virtual void write(bool openNewFile) ;
  } ;

}

#endif
