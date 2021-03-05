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

#ifndef USER_EVENTS_TRACE_WRITER_DOT_H
#define USER_EVENTS_TRACE_WRITER_DOT_H

#include "xdp/profile/writer/vp_base/vp_trace_writer.h"

namespace xdp {

  class UserEventsTraceWriter : public VPTraceWriter
  {
  private:
    UserEventsTraceWriter() = delete ;

    unsigned int bucketId ;
  protected:
    virtual void writeHeader() ;
    virtual void writeStructure() ;
    virtual void writeStringTable() ;
    virtual void writeTraceEvents() ;
    virtual void writeDependencies() ;

    virtual bool isHost() { return true ; }

  public:
    UserEventsTraceWriter(const char* filename) ;
    ~UserEventsTraceWriter() ;

    virtual bool write(bool openNewFile) ;
    
  } ;

} // end namespace xdp

#endif
