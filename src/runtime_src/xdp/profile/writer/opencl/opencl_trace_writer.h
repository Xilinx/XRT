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

#ifndef OPENCL_TRACE_WRITER_DOT_H
#define OPENCL_TRACE_WRITER_DOT_H

#include <map>
#include <vector>

#include "xdp/profile/writer/vp_base/vp_trace_writer.h"

namespace xdp {

  class OpenCLTraceWriter : public VPTraceWriter
  {
  private:
    OpenCLTraceWriter() = delete ;

    // Mappings of all the event types to bucket numbers
    std::map<uint64_t, int> commandQueueToBucket ;
    int generalAPIBucket ;
    int readBucket ;
    int writeBucket ;
    int copyBucket ;
    std::map<std::string, int> enqueueBuckets ;

    void setupBuckets() ;

    // A helper function to collapse dependency chains when some events
    //  are missing.
    void collapseDependencyChains(std::map<uint64_t, std::vector<uint64_t>>& d);

    void writeHumanReadableHeader() ;
    void writeHumanReadableStructure() ;
    void writeHumanReadableStringTable() ;
    void writeHumanReadableTraceEvents() ;
    void writeHumanReadableDependencies() ;

    void writeBinaryHeader() ;
    void writeBinaryStructure() ;
    void writeBinaryStringTable() ;
    void writeBinaryTraceEvents() ;
    void writeBinaryDependencies() ;

  protected:
    virtual void writeHeader() ;
    virtual void writeStructure() ;
    virtual void writeStringTable() ;
    virtual void writeTraceEvents() ;
    virtual void writeDependencies() ;

    virtual bool isHost() { return true ; } 

  public:
    OpenCLTraceWriter(const char* filename) ;
    ~OpenCLTraceWriter() ;

    virtual void write(bool openNewFile) ;
  } ;

}

#endif
