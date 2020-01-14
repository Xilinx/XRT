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

#include "xdp/profile/database/events/opencl_host_events.h"

namespace xdp {

  // **************************
  // Host event definitions
  // **************************
  KernelEnqueue::KernelEnqueue(uint64_t s_id, double ts) :
    VTFEvent(s_id, ts, KERNEL_ENQUEUE)
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
    VTFEvent(s_id, ts, CU_ENQUEUE) 
  {
  }

  CUEnqueue::~CUEnqueue()
  {
  }

  BufferTransfer::BufferTransfer(uint64_t s_id, double ts, VTFEventType ty) :
    VTFEvent(s_id, ts, ty) 
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
