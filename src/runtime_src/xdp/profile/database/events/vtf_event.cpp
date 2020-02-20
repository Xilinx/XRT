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

#include <fstream>
#include <iomanip>

#define XDP_SOURCE

#include "xdp/profile/database/events/vtf_event.h"

namespace xdp {

  // **************************
  // Base class definitions
  // **************************

  VTFEvent::VTFEvent(uint64_t s_id, double ts, VTFEventType ty) :
    id(0), start_id(s_id), timestamp(ts), type(ty)
  {
  }

  VTFEvent::~VTFEvent()
  {
  }

  void VTFEvent::dump(std::ofstream& fout, int bucket)
  {
    fout << id << "," << start_id << "," ;
    dumpTimestamp(fout) ;
    fout << "," << bucket << "," ;
    dumpType(fout, true) ;    
  }

  void VTFEvent::dumpTimestamp(std::ofstream& fout)
  {
    // Host events are accurate up to microseconds.
    //  Timestamps are in milliseconds, so the precision should be 3 past
    //  the decimal point
    std::ios_base::fmtflags flags = fout.flags() ;
    fout << std::fixed << std::setprecision(6) << (timestamp/1.0e6) ;
    fout.flags(flags) ;
  }

  void VTFEvent::dumpType(std::ofstream& fout, bool humanReadable)
  {
    switch (type)
    {
    case USER_MARKER:
      humanReadable ? (fout << "USER_MARKER") : (fout << USER_MARKER) ;
      break ;
    case USER_RANGE:
      humanReadable ? (fout << "USER_RANGE") : (fout << USER_RANGE) ;
      break ;
    case KERNEL_ENQUEUE:
      humanReadable ? (fout << "KERNEL_ENQUEUE") : (fout << KERNEL_ENQUEUE) ;
      break ;
    case CU_ENQUEUE:
      humanReadable ? (fout << "CU_ENQUEUE") : (fout << CU_ENQUEUE) ;
      break ;
    case READ_BUFFER:
      humanReadable ? (fout << "READ_BUFFER") : (fout << READ_BUFFER) ;
      break ;
    case READ_BUFFER_P2P:
      humanReadable ? (fout << "READ_BUFFER_P2P") : (fout << READ_BUFFER_P2P) ;
      break ;
    case WRITE_BUFFER:
      humanReadable ? (fout << "WRITE_BUFFER") : (fout << WRITE_BUFFER) ;
      break ;
    case WRITE_BUFFER_P2P:
      humanReadable ? (fout << "WRITE_BUFFER_P2P") : (fout << WRITE_BUFFER_P2P);
      break ;
    case COPY_BUFFER:
      humanReadable ? (fout << "COPY_BUFFER") : (fout << COPY_BUFFER) ;
      break ;
    case COPY_BUFFER_P2P:
      humanReadable ? (fout << "COPY_BUFFER_P2P") : (fout << COPY_BUFFER_P2P) ;
      break ;
    case OPENCL_API_CALL:
      humanReadable ? (fout << "OPENCL_API_CALL") : (fout << OPENCL_API_CALL) ;
      break ;
    case STREAM_READ:
      humanReadable ? (fout << "STREAM_READ") : (fout << STREAM_READ) ;
      break ;
    case STREAM_WRITE:
      humanReadable ? (fout << "STREAM_WRITE") : (fout << STREAM_WRITE) ;
      break ;
    case LOP_READ_BUFFER:
      humanReadable ? (fout << "LOP_READ_BUFFER") : (fout << LOP_READ_BUFFER) ;
      break ;
    case LOP_WRITE_BUFFER:
      humanReadable ? (fout << "LOP_WRITE_BUFFER") : (fout << LOP_WRITE_BUFFER);
      break ;
    case LOP_KERNEL_ENQUEUE:
      humanReadable ? (fout << "LOP_KERNEL_ENQUEUE") : (fout << LOP_KERNEL_ENQUEUE) ;
      break ;
    case KERNEL:
      humanReadable ? (fout << "KERNEL") : (fout << KERNEL) ;
      break ;
    case KERNEL_STALL:
      humanReadable ? (fout << "KERNEL_STALL") : (fout << KERNEL_STALL) ;
      break ;
    case KERNEL_READ:
      humanReadable ? (fout << "KERNEL_READ") : (fout << KERNEL_READ) ;
      break ;
    case KERNEL_WRITE:
      humanReadable ? (fout << "KERNEL_WRITE") : (fout << KERNEL_WRITE) ;
      break ;
    case KERNEL_STREAM_READ:
      humanReadable ? (fout << "KERNEL_STREAM_READ") : (fout << KERNEL_STREAM_READ) ;
      break ;
    case KERNEL_STREAM_WRITE:
      humanReadable ? (fout << "KERNEL_STREAM_WRITE") : (fout << KERNEL_STREAM_WRITE) ;
      break ;
    case KERNEL_STREAM_STALL:
      humanReadable ? (fout << "KERNEL_STREAM_STALL") : (fout << KERNEL_STREAM_STALL) ;
      break ;
    case KERNEL_STREAM_STARVE:
      humanReadable ? (fout << "KERNEL_STREAM_STARVE") : (fout << KERNEL_STREAM_STARVE) ;
      break ;
    case HOST_READ:
      humanReadable ? (fout << "HOST_READ") : (fout << HOST_READ) ;
      break ;
    case HOST_WRITE:
      humanReadable ? (fout << "HOST_WRITE") : (fout << HOST_WRITE) ;
      break ;
    case HAL_API_CALL:
      humanReadable ? (fout << "HAL_API_CALL") : (fout << HAL_API_CALL) ;
      break ;
    default:
      humanReadable ? (fout << "UNKNOWN") : (fout << -1) ;
      break ;
    }
  }

  // **************************
  // API Call definitions
  // **************************

  APICall::APICall(uint64_t s_id, double ts, unsigned int f_id, uint64_t name,
		   VTFEventType ty) :
    VTFEvent(s_id, ts, ty), functionId(f_id), functionName(name)
  {
  }

  APICall::~APICall()
  {
  }

}

