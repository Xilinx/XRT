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
    uint64_t deviceName ; // string
    uint64_t binaryName ; // string
    uint64_t kernelName ; // string
    uint64_t workgroupConfiguration ; // string
    size_t workgroupSize ;

    std::string identifier ;

    KernelEnqueue() = delete ;
  public:
    XDP_EXPORT KernelEnqueue(uint64_t s_id, double ts, 
			     uint64_t dName, uint64_t bName, uint64_t kName,
			     uint64_t wgc, size_t wgs,
			     const char* enqueueId) ;

    XDP_EXPORT ~KernelEnqueue() ;

    inline std::string getIdentifier() { return identifier ; }

    virtual bool isHostEvent() { return true ; }
    virtual bool isOpenCLHostEvent() { return true ; }
    
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

  /*
  class CUEnqueue : public VTFEvent
  {
  private:
    // These will be used to determine the bucket in the writer
    uint64_t deviceName ;
    uint64_t binaryName ;
    uint64_t kernelName ;
    uint64_t workgroupConfiguration ;
    uint64_t cuName ;
    //uint64_t objId ;
    size_t size ;
    uint64_t cuId ;

    CUEnqueue() = delete ;
  public:
    XDP_EXPORT CUEnqueue(uint64_t s_id, double ts) ;
    XDP_EXPORT ~CUEnqueue() ;

    virtual bool isHostEvent() { return true ; } 
  } ;
  */
  class BufferTransfer : public VTFEvent // For HAL level
  {
  private:
    size_t size ;

    BufferTransfer() = delete ;
  public:
    XDP_EXPORT BufferTransfer(uint64_t s_id, double ts, VTFEventType ty,
                              size_t bufSz = 0);
    XDP_EXPORT ~BufferTransfer() ;

    virtual bool isHostEvent() { return true ; } 

    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket) ;
  } ;

  class OpenCLBufferTransfer : public VTFEvent
  {
  private:
    std::thread::id threadId ;
    uint64_t deviceAddress ;
    uint64_t memoryResource ; // string
    size_t bufferSize ;

    OpenCLBufferTransfer() = delete ;
  public:
    XDP_EXPORT OpenCLBufferTransfer(uint64_t s_id, double ts, VTFEventType ty,
				    uint64_t address, uint64_t resource,
				    size_t size) ;
    XDP_EXPORT ~OpenCLBufferTransfer() ;

    virtual bool isHostEvent()       { return true ; }
    virtual bool isOpenCLHostEvent() { return true ; }

    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket) ;
  } ;

  class OpenCLCopyBuffer : public VTFEvent
  {
  private:
    std::thread::id threadId ;
    uint64_t srcDeviceAddress ;
    uint64_t srcMemoryResource ; // string
    uint64_t dstDeviceAddress ;
    uint64_t dstMemoryResource ; // string
    size_t bufferSize ;

    OpenCLCopyBuffer() = delete ;
  public:
    XDP_EXPORT OpenCLCopyBuffer(uint64_t s_id, double ts, VTFEventType ty,
				uint64_t srcAddress, uint64_t srcResource,
				uint64_t dstAddress, uint64_t dstResource,
				size_t size) ;
    XDP_EXPORT ~OpenCLCopyBuffer() ;

    virtual bool isHostEvent()       { return true ; }
    virtual bool isOpenCLHostEvent() { return true ; }

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
