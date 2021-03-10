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

#include "xocl/config.h"
#include "rt_printf.h"
#include "xocl/core/kernel.h"

#ifdef _WIN32
# pragma warning( disable : 4996 )
#endif

namespace XCL {
namespace Printf {


PrintfManager::PrintfManager()
{
}

PrintfManager::~PrintfManager()
{
  m_queue.clear();
}

void PrintfManager::enqueueBuffer(cl_kernel kernel, const std::vector<uint8_t>& buf)
{
  BufferPrintf bp(buf, xocl::xocl(kernel)->get_stringtable());
  m_queue.push_back(bp);
}

void PrintfManager::clear()
{
  m_queue.clear();
}

void PrintfManager::print(std::ostream& os)
{
  for ( BufferPrintf& bp : m_queue ) {
    bp.print(os);
  }
}

void PrintfManager::dbgDump(std::ostream& os)
{
  for ( BufferPrintf& bp : m_queue ) {
    bp.dbgDump(os);
  }
}

/////////////////////////////////////////////////////////////////////////

bool kernelHasPrintf(cl_kernel kernel)
{
  bool retval = (kernel && xocl::xocl(kernel)->has_printf() && (xocl::xocl(kernel)->get_stringtable().size() > 0) );
  return retval;
}

bool isPrintfDebugMode()
{
  bool retval = false;
  char *p_bufEnv = getenv("XCL_PRINTF_DEBUG");
  if (p_bufEnv != nullptr) {
    retval = true;
  }
  return retval;
}

} // namespace Printf
} // namespace XCL
