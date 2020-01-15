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

#ifndef OPENCL_HOST_EVENTS_DOT_H
#define OPENCL_HOST_EVENTS_DOT_H

#include <thread>
#include <fstream>

#include "xdp/profile/database/events/vtf_event.h"

namespace xdp {

  // **********************
  // Host events
  // **********************
  class KernelEnqueue : public VTFEvent
  {
  private:
    uint64_t deviceName ;
    uint64_t binaryName ;
    uint64_t kernelName ;
    uint64_t workgroupConfiguration ;
    uint64_t workgroupSize ;
    uint64_t eventString ;
    uint64_t stageString ;
    uint64_t objId ;
    size_t size ;

    KernelEnqueue() = delete ;
  public:
    KernelEnqueue(uint64_t s_id, double ts);
    ~KernelEnqueue() ;

    virtual bool isHostEvent() { return true ; }
    
    virtual void dump(std::ofstream& fout, int bucket) ;
  } ;

  class CUEnqueue : public VTFEvent
  {
  private:
    uint64_t deviceName ;
    uint64_t binaryName ;
    uint64_t kernelName ;
    uint64_t workgroupConfiguration ;
    uint64_t cuName ;
    uint64_t eventString ;
    uint64_t stageString ;
    uint64_t objId ;
    size_t size ;
    uint64_t cuId ;

    CUEnqueue() = delete ;
  public:
    CUEnqueue(uint64_t s_id, double ts) ;
    ~CUEnqueue() ;

    virtual bool isHostEvent() { return true ; } 
  } ;

  class BufferTransfer : public VTFEvent
  {
  private:
    uint64_t stageString ;
    uint64_t eventString ;
    size_t size ;
    uint64_t srcAddress ;
    uint64_t srcBank ;
    uint64_t dstAddress ;
    uint64_t dstBank ;
    std::thread::id threadId ;
    uint64_t bufferId ;

    BufferTransfer() = delete ;
  public:
    BufferTransfer(uint64_t s_id, double ts, VTFEventType ty) ;
    ~BufferTransfer() ;

    virtual bool isHostEvent() { return true ; } 

    virtual void dump(std::ofstream& fout, int bucket) ;
  } ;

  class StreamRead : public VTFEvent
  {
  private:
    StreamRead() = delete ;
  public:
    StreamRead(uint64_t s_id, double ts) ;
    ~StreamRead() ;

    virtual bool isHostEvent() { return true ; } 
  } ;

  class StreamWrite : public VTFEvent
  {
  private:
    StreamWrite() = delete ;
  public:
    StreamWrite(uint64_t s_id, double ts) ;
    ~StreamWrite() ;

    virtual bool isHostEvent() { return true ; } 
  } ;

} // end namespace xdp

#endif
