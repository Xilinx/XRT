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

#include "xdp/config.h"

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
    XDP_EXPORT KernelEnqueue(uint64_t s_id, double ts);
    XDP_EXPORT ~KernelEnqueue() ;

    virtual bool isHostEvent() { return true ; }
    
    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket) ;
  } ;

  class LOPKernelEnqueue : public VTFEvent
  {
  private:
    LOPKernelEnqueue() = delete ;
  public:
    XDP_EXPORT LOPKernelEnqueue(uint64_t s_id, double ts) ;
    XDP_EXPORT ~LOPKernelEnqueue() ;

    virtual bool isHostEvent() { return true ; }
    virtual bool isLOPHostEvent() { return true ; }

    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket) ;
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
    XDP_EXPORT CUEnqueue(uint64_t s_id, double ts) ;
    XDP_EXPORT ~CUEnqueue() ;

    virtual bool isHostEvent() { return true ; } 
  } ;

  class BufferTransfer : public VTFEvent
  {
  private:
    #if 0
    uint64_t stageString ;
    uint64_t eventString ;
    #endif
    size_t size ;
    #if 0
    uint64_t srcAddress ;
    uint64_t srcBank ;
    uint64_t dstAddress ;
    uint64_t dstBank ;
    std::thread::id threadId ;
    uint64_t bufferId ;
    #endif

    BufferTransfer() = delete ;
  public:
    XDP_EXPORT BufferTransfer(uint64_t s_id, double ts, VTFEventType ty,
                              size_t bufSz = 0);
    XDP_EXPORT ~BufferTransfer() ;

    virtual bool isHostEvent() { return true ; } 

    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket) ;
  } ;

  class LOPBufferTransfer : public VTFEvent
  {
  private:
    std::thread::id threadId ;
  public:
    XDP_EXPORT LOPBufferTransfer(uint64_t s_id, double ts, VTFEventType ty) ;
    XDP_EXPORT ~LOPBufferTransfer() ;

    virtual bool isHostEvent() { return true ; }
    virtual bool isLOPHostEvent() { return true ; }

    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket) ;
  } ;

  class StreamRead : public VTFEvent
  {
  private:
    StreamRead() = delete ;
  public:
    XDP_EXPORT StreamRead(uint64_t s_id, double ts) ;
    XDP_EXPORT ~StreamRead() ;

    virtual bool isHostEvent() { return true ; } 
  } ;

  class StreamWrite : public VTFEvent
  {
  private:
    StreamWrite() = delete ;
  public:
    XDP_EXPORT StreamWrite(uint64_t s_id, double ts) ;
    XDP_EXPORT ~StreamWrite() ;

    virtual bool isHostEvent() { return true ; } 
  } ;

} // end namespace xdp

#endif
