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
  KernelEnqueue::KernelEnqueue(uint64_t s_id, double ts) :
    VTFEvent(s_id, ts, KERNEL_ENQUEUE),
    // Until implemented, initialize all members with a default value
    deviceName(0), binaryName(0), kernelName(0),
    workgroupConfiguration(0), workgroupSize(0),
    eventString(0), stageString(0), objId(0), size(0)
  {
  }

  KernelEnqueue::~KernelEnqueue()
  {
  }

  void KernelEnqueue::dump(std::ofstream& fout, int bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    fout << std::endl; 
  }

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

  BufferTransfer::BufferTransfer(uint64_t s_id, double ts, VTFEventType ty) :
    VTFEvent(s_id, ts, ty),
    // Until implemented, initialize all members with a default value
    stageString(0), eventString(0), size(0), srcAddress(0), srcBank(0),
    dstAddress(0), dstBank(0), bufferId(0)
  {
  }

  BufferTransfer::~BufferTransfer()
  {
  }

  void BufferTransfer::dump(std::ofstream& fout, int bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    fout << std::endl ;
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
