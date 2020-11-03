/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

// Copyright 2015 Xilinx, Inc. All rights reserved.
#ifndef __XILINX_RT_PRINTF_H
#define __XILINX_RT_PRINTF_H

#include "rt_printf_impl.h"
#include <CL/opencl.h>
#include <iostream>
#include <vector>
#include <map>
#include <string>

/////////////////////////////////////////////////////////////////////////
// rt_printf.h
//
// SDAccel Printf Manager - accepts print buffers and string tables from
// workgroup completion events. Sends printf output to stdout at
// periodic times from the event scheduler thread.
/////////////////////////////////////////////////////////////////////////

namespace XCL {
namespace Printf {

class PrintfManager
{
public:
  PrintfManager();
  ~PrintfManager();

  void enqueueBuffer(cl_kernel kernel, const std::vector<uint8_t>& buf);
  void clear();
  void print(std::ostream& os = std::cout);
  void dbgDump(std::ostream& os = std::cout);

private:
  std::vector<BufferPrintf> m_queue;

};

/////////////////////////////////////////////////////////////////////////
// UTILITY FUNCTIONS

bool kernelHasPrintf(cl_kernel kernel);
bool isPrintfDebugMode();

/////////////////////////////////////////////////////////////////////////

} // namespace Printf
} // namespace XCL

#endif
