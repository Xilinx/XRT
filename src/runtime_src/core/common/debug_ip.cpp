/**
 * Copyright (C) 2021-2022 Xilinx, Inc
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

#define XRT_CORE_COMMON_SOURCE

#include "debug_ip.h"

//#include "core/common/xrt_profiling.h"
#include "core/include/xcl_perfmon_parameters.h"
//#include "core/include/xcl_axi_checker_codes.h"
//#include "core/include/experimental/xrt-next.h"

namespace xrt_core { namespace debug_ip {

std::vector<uint64_t> getAIMCounterResult(const xrt_core::device* device, debug_ip_data* dbgIpData)
{
  // read counter values
  static const uint64_t aim_offsets[] = {
    XAIM_SAMPLE_WRITE_BYTES_OFFSET,
    XAIM_SAMPLE_WRITE_TRANX_OFFSET,
    XAIM_SAMPLE_READ_BYTES_OFFSET,
    XAIM_SAMPLE_READ_TRANX_OFFSET,
    XAIM_SAMPLE_OUTSTANDING_COUNTS_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_ADDRESS_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_DATA_OFFSET,
    XAIM_SAMPLE_LAST_READ_ADDRESS_OFFSET,
    XAIM_SAMPLE_LAST_READ_DATA_OFFSET
  };

  static const uint64_t aim_upper_offsets[] = {
    XAIM_SAMPLE_WRITE_BYTES_UPPER_OFFSET,
    XAIM_SAMPLE_WRITE_TRANX_UPPER_OFFSET,
    XAIM_SAMPLE_READ_BYTES_UPPER_OFFSET,
    XAIM_SAMPLE_READ_TRANX_UPPER_OFFSET,
    XAIM_SAMPLE_OUTSTANDING_COUNTS_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_ADDRESS_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_DATA_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_READ_ADDRESS_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_READ_DATA_UPPER_OFFSET
  };

  result_type retvalBuf;

  uint32_t currData[XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT];

  uint32_t sampleInterval;

  // Read sample interval register to latch the sampled metric counters
  device->xread(dbgIpData->m_base_address + XAIM_SAMPLE_OFFSET,
                    &sampleInterval, sizeof(uint32_t));
#if 0
  xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
                    dbgIpData->m_base_address + XAIM_SAMPLE_OFFSET,
                    &sampleInterval, sizeof(uint32_t));
#endif

 // If applicable, read the upper 32-bits of the 64-bit debug counters
  if (dbgIpData->m_properties & XAIM_64BIT_PROPERTY_MASK) {
    for (int c = 0 ; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
      device->xread(dbgIpData->m_base_address + aim_upper_offsets[c],
                        &currData[c], sizeof(uint32_t));
#if 0
      xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
                 dbgIpData->m_base_address + aim_upper_offsets[c], &currData[c], sizeof(uint32_t));
#endif
    }
    retvalBuf.push_back(((uint64_t)(currData[0])) << 32 );
    retvalBuf.push_back(((uint64_t)(currData[1])) << 32 );
    retvalBuf.push_back(((uint64_t)(currData[2])) << 32 );
    retvalBuf.push_back(((uint64_t)(currData[3])) << 32 );
    retvalBuf.push_back(((uint64_t)(currData[4])) << 32 );
    retvalBuf.push_back(((uint64_t)(currData[5])) << 32 );
    retvalBuf.push_back(((uint64_t)(currData[6])) << 32 );
    retvalBuf.push_back(((uint64_t)(currData[7])) << 32 );
    retvalBuf.push_back(((uint64_t)(currData[8])) << 32 );
  }

  for (int c=0; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++) {
    device->xread(dbgIpData->m_base_address + aim_offsets[c],
                      &currData[c], sizeof(uint32_t));
#if 0
    xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
                       dbgIpData->m_base_address + aim_offsets[c], &currData[c], sizeof(uint32_t));
#endif
  }

  retvalBuf[0] |= currData[0];
  retvalBuf[1] |= currData[1];
  retvalBuf[2] |= currData[2];
  retvalBuf[3] |= currData[3];
  retvalBuf[4] |= currData[4];
  retvalBuf[5] |= currData[5];
  retvalBuf[6] |= currData[6];
  retvalBuf[7] |= currData[7];
  retvalBuf[8] |= currData[8];

  return retvalBuf;

}

} }
