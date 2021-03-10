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

#define XDP_SOURCE

#include "xdp/profile/database/events/opencl_host_events.h"

namespace xdp {

  // **************************
  // Host event definitions
  // **************************
  KernelEnqueue::KernelEnqueue(uint64_t s_id, double ts, 
			       uint64_t dName, 
			       uint64_t bName, 
			       uint64_t kName,
			       uint64_t wgc,
			       size_t wgs,
			       const char* enqueueId) :
    VTFEvent(s_id, ts, KERNEL_ENQUEUE),
    deviceName(dName), binaryName(bName), kernelName(kName),
    workgroupConfiguration(wgc),
    workgroupSize(wgs),
    identifier("")
  {
    if (enqueueId != nullptr) identifier = enqueueId ;
  }

  KernelEnqueue::~KernelEnqueue()
  {
  }

  void KernelEnqueue::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    fout << "," << kernelName ;
    fout << "," << workgroupConfiguration ;
    fout << "," << workgroupSize ;
    fout << "," << 0 ; // This is the "size"
    fout << std::endl; 
  }

  LOPKernelEnqueue::LOPKernelEnqueue(uint64_t s_id, double ts) :
    VTFEvent(s_id, ts, LOP_KERNEL_ENQUEUE)
  {
  }

  LOPKernelEnqueue::~LOPKernelEnqueue()
  {
  }

  void LOPKernelEnqueue::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    fout << std::endl ;
  }

  /*
  CUEnqueue::CUEnqueue(uint64_t s_id, double ts) :
    VTFEvent(s_id, ts, CU_ENQUEUE),
    // Until implemented, initialize all members with a default value
    deviceName(0), binaryName(0), kernelName(0),
    workgroupConfiguration(0), cuName(0), eventString(0),
    stageString(0), objId(0), size(0), cuId(0)
  {
  }

  CUEnqueue::~CUEnqueue()
  {
  }
  */

  BufferTransfer::BufferTransfer(uint64_t s_id, double ts, VTFEventType ty,
                                 size_t bufSz)
    : VTFEvent(s_id, ts, ty), size(bufSz)
  {
  }

  BufferTransfer::~BufferTransfer()
  {
  }

  void BufferTransfer::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket);
    if(0 == start_id) {  // Dump the detailed information only for start event
      fout << "," << size;
    }
    fout << std::endl;
  }

  OpenCLBufferTransfer::OpenCLBufferTransfer(uint64_t s_id, double ts,
					     VTFEventType ty,
					     uint64_t address,
					     uint64_t resource,
					     size_t size)
    :VTFEvent(s_id, ts, ty), threadId(std::this_thread::get_id()),
     deviceAddress(address), memoryResource(resource),
     bufferSize(size)
  {
  }

  OpenCLBufferTransfer::~OpenCLBufferTransfer()
  {
  }

  void OpenCLBufferTransfer::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    if (0 == start_id) // Dump the detailed information only for start event
    {
      fout << "," << bufferSize ;
      fout << ",0x" << std::hex << deviceAddress << std::dec ;
      fout << "," << memoryResource ;
      fout << ",0x" << std::hex << threadId << std::dec ;
    }
    fout << std::endl ;
  }


  OpenCLCopyBuffer::OpenCLCopyBuffer(uint64_t s_id, double ts, VTFEventType ty,
				     uint64_t srcAddress, uint64_t srcResource,
				     uint64_t dstAddress, uint64_t dstResource,
				     size_t size)
    :VTFEvent(s_id, ts, ty), threadId(std::this_thread::get_id()),
     srcDeviceAddress(srcAddress), srcMemoryResource(srcResource),
     dstDeviceAddress(dstAddress), dstMemoryResource(dstResource),
     bufferSize(size)
  {
  }

  OpenCLCopyBuffer::~OpenCLCopyBuffer()
  {
  }

  void OpenCLCopyBuffer::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    if (0 == start_id) // Dump the detailed information only for start event
    {
      fout << "," << 1 ; // Transfer type
      fout << "," << bufferSize 
	   << "," << srcDeviceAddress
	   << "," << srcMemoryResource
	   << "," << dstDeviceAddress
	   << "," << dstMemoryResource 
	   << ",0x" << std::hex << threadId << std::dec ;
    }
    fout << std::endl ;
  }

  LOPBufferTransfer::LOPBufferTransfer(uint64_t s_id, double ts, 
				       VTFEventType ty) :
    VTFEvent(s_id, ts, ty), threadId(std::this_thread::get_id())
  {
    
  }

  LOPBufferTransfer::~LOPBufferTransfer()
  {
  }

  void LOPBufferTransfer::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    fout << "," << std::hex << "0x" << threadId << std::dec << std::endl ;
  }

  StreamRead::StreamRead(uint64_t s_id, double ts) :
    VTFEvent(s_id, ts, STREAM_READ)
  {
  }

  StreamRead::~StreamRead()
  {
  }

  StreamWrite::StreamWrite(uint64_t s_id, double ts) :
    VTFEvent(s_id, ts, STREAM_WRITE)
  {
  }

  StreamWrite::~StreamWrite()
  {
  }

} // end namespace xdp
