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

#ifndef VP_TRACE_WRITER_DOT_H
#define VP_TRACE_WRITER_DOT_H

#include <string>

#include "xdp/profile/writer/vp_writer.h"
#include "xdp/config.h"

namespace xdp {
  
  class VPTraceWriter : public VPWriter
  {
  private:
    VPTraceWriter() = delete ;

    // Header information that all Trace writers use
    std::string version ;
    // Also PID, which is stored in the database
    std::string creationTime ;
    uint16_t resolution ;
    
  protected:
    // Each new trace CSV file has the following sections
    XDP_EXPORT virtual void writeHeader() ;
    virtual void writeStructure() = 0 ;
    virtual void writeStringTable() = 0 ;
    virtual void writeTraceEvents() = 0 ;
    virtual void writeDependencies() = 0 ;

    // Trace formats can either be dumped as a binary or human readable
    bool humanReadable ;

    // The different types of VTF file formats supported
    virtual bool isHost()   { return false ; }
    virtual bool isDevice() { return false ; }
    virtual bool isAIE()    { return false ; }
    virtual bool isKernel() { return false ; } 

  public:
    XDP_EXPORT VPTraceWriter(const char* filename, const std::string& v,
			     const std::string& c, uint16_t r) ;
    XDP_EXPORT ~VPTraceWriter() ;

    void setHumanReadable() { humanReadable = true ; } 
  } ;
  
}

#endif
