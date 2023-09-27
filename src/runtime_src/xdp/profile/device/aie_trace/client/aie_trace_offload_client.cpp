/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include <iostream>

#include "core/common/message.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/device/aie_trace/aie_trace_logger.h"
#include "xdp/profile/device/aie_trace/aie_trace_offload_client.h"
#include "xdp/profile/device/device_intf.h"


namespace xdp {


AIETraceOffload::AIETraceOffload
  ( void* handle, uint64_t id
  , DeviceIntf* dInt
  , AIETraceLogger* logger
  , bool isPlio
  , uint64_t totalSize
  , uint64_t numStrm
  )
  : deviceHandle(handle)
  , deviceId(id)
  , deviceIntf(dInt)
  , traceLogger(logger)
  , isPLIO(isPlio)
  , totalSz(totalSize)
  , numStream(numStrm)
  , traceContinuous(false)
  , offloadIntervalUs(0)
  , bufferInitialized(false)
  , offloadStatus(AIEOffloadThreadStatus::IDLE)
  , mEnCircularBuf(false)
  , mCircularBufOverwrite(false)
{
  
}

AIETraceOffload::~AIETraceOffload()
{

}



bool AIETraceOffload::initReadTrace()
{
  
}

void AIETraceOffload::endReadTrace()
{
 
}

void AIETraceOffload::readTraceGMIO(bool final)
{
  
}

void AIETraceOffload::readTracePLIO(bool final)
{
  
}

uint64_t AIETraceOffload::syncAndLog(uint64_t index)
{
  
}

bool AIETraceOffload::isTraceBufferFull()
{
  
}

void AIETraceOffload::checkCircularBufferSupport()
{
  
}

void AIETraceOffload::startOffload()
{
  
}

void AIETraceOffload::continuousOffload()
{
  
}

bool AIETraceOffload::keepOffloading()
{
  
}

void AIETraceOffload::stopOffload()
{
  
}

void AIETraceOffload::offloadFinished()
{
  
}

uint64_t AIETraceOffload::searchWrittenBytes(void* buf, uint64_t bytes)
{
  
}

}

