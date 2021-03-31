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

#ifndef VTF_EVENT_DOT_H
#define VTF_EVENT_DOT_H

#include <fstream>

#include "xdp/config.h"

namespace xdp {

  enum VTFEventType {
    // User level events
    USER_MARKER          = 0,
    USER_RANGE           = 1,

    // OpenCL host level events
    KERNEL_ENQUEUE       = 10,
    CU_ENQUEUE           = 11,
    READ_BUFFER          = 12,
    READ_BUFFER_P2P      = 13,
    WRITE_BUFFER         = 14,
    WRITE_BUFFER_P2P     = 15,
    COPY_BUFFER          = 16,
    COPY_BUFFER_P2P      = 17,
    OPENCL_API_CALL      = 18,
    STREAM_READ          = 19,
    STREAM_WRITE         = 20,

    // Low overhead OpenCL host level events
    LOP_READ_BUFFER      = 21,
    LOP_WRITE_BUFFER     = 22,
    LOP_KERNEL_ENQUEUE   = 23,

    // PL events
    KERNEL                     = 30,
    KERNEL_STALL               = 31,
    KERNEL_STALL_EXT_MEM       = 32,
    KERNEL_STALL_DATAFLOW      = 33,
    KERNEL_STALL_PIPE          = 34,
    KERNEL_READ                = 35,
    KERNEL_WRITE               = 36,
    KERNEL_STREAM_READ         = 37,
    KERNEL_STREAM_READ_STALL   = 38,
    KERNEL_STREAM_READ_STARVE  = 39,
    KERNEL_STREAM_WRITE        = 40,
    KERNEL_STREAM_WRITE_STALL  = 41,
    KERNEL_STREAM_WRITE_STARVE = 42,
    HOST_READ                  = 43,
    HOST_WRITE                 = 44,
    XCLBIN_END                 = 45,

    // AIE events

    // XRT host level events
    API_CALL             = 50,
    HAL_API_CALL         = 51,
    NATIVE_API_CALL      = 52,
  } ;

  class VTFEvent
  {
  private:
    VTFEvent() = delete ;

  protected:
    // Every trace event has the following four fields:
    uint64_t id ;       // Assigned by the database when it is entered
    uint64_t start_id ; // 0 if this is a start event,
    double   timestamp ; // aligned timestamp
    VTFEventType type ; // For quick lookup

    virtual void dumpTimestamp(std::ofstream& fout) ;
    void dumpType(std::ofstream& fout, bool humanReadable) ;

  public:
    XDP_EXPORT VTFEvent(uint64_t s_id, double ts, VTFEventType ty) ;
    XDP_EXPORT virtual ~VTFEvent() ;

    // Getters and Setters
    inline double       getTimestamp()    const { return timestamp ; }
    inline void         setTimestamp(double ts) { timestamp = ts ; }
    inline uint64_t     getEventId()            { return id ; }
    inline void         setEventId(uint64_t i)  { id = i ; }
    inline VTFEventType getEventType()          { return type; }

    // Functions that can be used as filters
    virtual bool isUserEvent()       { return false ; }
    virtual bool isOpenCLAPI()       { return false ; } 
    virtual bool isHALAPI()          { return false ; }
    virtual bool isHostEvent()       { return false ; }
    virtual bool isNativeHostEvent() { return false ; } 
    virtual bool isOpenCLHostEvent()
      { return type == READ_BUFFER  || type == READ_BUFFER_P2P  ||
               type == WRITE_BUFFER || type == WRITE_BUFFER_P2P ||
	       type == KERNEL_ENQUEUE ; }
    virtual bool isLOPHostEvent() { return false ; }
    virtual bool isHALHostEvent()
      { return type == READ_BUFFER  ||
               type == WRITE_BUFFER ||
	       type == KERNEL_ENQUEUE ; }
    virtual bool isDeviceEvent() { return false ; }
    virtual bool isReadBuffer()  { return type == READ_BUFFER || 
	                                  type == READ_BUFFER_P2P ||
	                                  type == LOP_READ_BUFFER ; }
    virtual bool isWriteBuffer() { return type == WRITE_BUFFER || 
	                                  type == WRITE_BUFFER_P2P ||
	                                  type == LOP_WRITE_BUFFER ; }
    virtual bool isCopyBuffer() { return type == COPY_BUFFER ||
                                         type == COPY_BUFFER_P2P ; }
    virtual bool isKernelEnqueue() { return type == KERNEL_ENQUEUE ||
	                                    type == LOP_KERNEL_ENQUEUE ; }

    virtual uint64_t getDevice() { return 0 ; } // CHECK
    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket) ;
  } ;

  // Used so the database can sort based on timestamp order
  class VTFEventSorter
  {
  public:
    bool operator() (VTFEvent* const& l, VTFEvent* const& r)
    {
      return (l->getTimestamp() < r->getTimestamp()) ;
    }
  } ;
 
  class APICall : public VTFEvent 
  {
  protected:
    uint64_t functionName ; // An index into the string table

    APICall() = delete ;
  public:
    XDP_EXPORT APICall(uint64_t s_id, double ts, uint64_t name, VTFEventType ty);
    XDP_EXPORT ~APICall() ;

    virtual bool isHostEvent() { return true ; } 
  } ;
 
}

#endif
