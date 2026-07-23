// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2015-2017 Xilinx, Inc. All rights reserved.
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#include "xocl/config.h"
#include "rt_printf.h"
#include "xocl/core/kernel.h"

#include "core/common/utils.h"

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
  return xrt_core::utils::is_env("XCL_PRINTF_DEBUG");
}

} // namespace Printf
} // namespace XCL
