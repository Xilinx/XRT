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

  std::vector<uint64_t> retvalBuf(9, 0);

  uint32_t currData[XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT];

  uint32_t sampleInterval;

  // Read sample interval register to latch the sampled metric counters
  device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, 
                    dbgIpData->m_base_address + XAIM_SAMPLE_OFFSET,
                    &sampleInterval, sizeof(uint32_t));

 // If applicable, read the upper 32-bits of the 64-bit debug counters
  if (dbgIpData->m_properties & XAIM_64BIT_PROPERTY_MASK) {
    for (int c = 0 ; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        dbgIpData->m_base_address + aim_upper_offsets[c],
                        &currData[c], sizeof(uint32_t));
    }
    retvalBuf[0] = (((uint64_t)(currData[0])) << 32 );
    retvalBuf[1] = (((uint64_t)(currData[1])) << 32 );
    retvalBuf[2] = (((uint64_t)(currData[2])) << 32 );
    retvalBuf[3] = (((uint64_t)(currData[3])) << 32 );
    retvalBuf[4] = (((uint64_t)(currData[4])) << 32 );
    retvalBuf[5] = (((uint64_t)(currData[5])) << 32 );
    retvalBuf[6] = (((uint64_t)(currData[6])) << 32 );
    retvalBuf[7] = (((uint64_t)(currData[7])) << 32 );
    retvalBuf[8] = (((uint64_t)(currData[8])) << 32 );
  }

  for (int c=0; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++) {
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      dbgIpData->m_base_address + aim_offsets[c],
                      &currData[c], sizeof(uint32_t));
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

std::vector<uint64_t> getAMCounterResult(const xrt_core::device* device, debug_ip_data* dbgIpData)
{
  // read counter values
  static const uint64_t am_offsets[] = {
    XAM_ACCEL_EXECUTION_COUNT_OFFSET,
    XAM_ACCEL_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_STALL_INT_OFFSET,
    XAM_ACCEL_STALL_STR_OFFSET,
    XAM_ACCEL_STALL_EXT_OFFSET,
    XAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_TOTAL_CU_START_OFFSET
  };

  static const uint64_t am_upper_offsets[] = {
    XAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET,
    XAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET,
    XAM_ACCEL_STALL_INT_UPPER_OFFSET,
    XAM_ACCEL_STALL_STR_UPPER_OFFSET,
    XAM_ACCEL_STALL_EXT_UPPER_OFFSET,
    XAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET,
    XAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET,
    XAM_ACCEL_TOTAL_CU_START_UPPER_OFFSET
  };

  std::vector<uint64_t> retvalBuf(10, 0);

  // Read all metric counters
  uint32_t currData[XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT] = {0};

  uint32_t sampleInterval;
  // Read sample interval register to latch the sampled metric counters
  device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                  dbgIpData->m_base_address + XAM_SAMPLE_OFFSET,
                  &sampleInterval, sizeof(uint32_t));

  auto dbgIpVersion = std::make_pair(dbgIpData->m_major, dbgIpData->m_minor);
  auto refVersion   = std::make_pair((uint8_t)1, (uint8_t)1);

  bool hasDataflow = (dbgIpVersion > refVersion) ? true : false;

  // If applicable, read the upper 32-bits of the 64-bit debug counters
  if (dbgIpData->m_properties & XAM_64BIT_PROPERTY_MASK) {
    for (int c = 0 ; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
            dbgIpData->m_base_address + am_upper_offsets[c],
            &currData[c], sizeof(uint32_t));
    }
    retvalBuf[0] = ((uint64_t)(currData[0])) << 32;

    retvalBuf[2] = ((uint64_t)(currData[1])) << 32;

    retvalBuf[5] = ((uint64_t)(currData[2])) << 32;

    retvalBuf[3] = ((uint64_t)(currData[3])) << 32;

    retvalBuf[4] = ((uint64_t)(currData[4])) << 32;

    retvalBuf[9] = ((uint64_t)(currData[5])) << 32;

    retvalBuf[8] = ((uint64_t)(currData[6])) << 32;

    retvalBuf[1] = ((uint64_t)(currData[7])) << 32;
    
#if 0

    amResults.CuExecCount[index]        = valBuf[0];
    amResults.CuStartCount[index]       = valBuf[1];
    amResults.CuExecCycles[index]       = valBuf[2];

    amResults.CuStallIntCycles[index]   = valBuf[3];
    amResults.CuStallStrCycles[index]   = valBuf[4];
    amResults.CuStallExtCycles[index]   = valBuf[5];

    amResults.CuBusyCycles[index]       = valBuf[6];
    amResults.CuMaxParallelIter[index]  = valBuf[7];
    amResults.CuMaxExecCycles[index]    = valBuf[8];
    amResults.CuMinExecCycles[index]    = valBuf[9];

    amResults.CuExecCount[index]      = ((uint64_t)(currData[0])) << 32;
    amResults.CuExecCycles[index]     = ((uint64_t)(currData[1])) << 32;
    amResults.CuStallExtCycles[index] = ((uint64_t)(currData[2])) << 32;
    amResults.CuStallIntCycles[index] = ((uint64_t)(currData[3])) << 32;
    amResults.CuStallStrCycles[index] = ((uint64_t)(currData[4])) << 32;
    amResults.CuMinExecCycles[index]  = ((uint64_t)(currData[5])) << 32;
    amResults.CuMaxExecCycles[index]  = ((uint64_t)(currData[6])) << 32;
    amResults.CuStartCount[index]     = ((uint64_t)(currData[7])) << 32;
#endif
    if(hasDataflow) {
      uint64_t dfTmp[2] = {0};
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpData->m_base_address + XAM_BUSY_CYCLES_UPPER_OFFSET, &dfTmp[0], sizeof(uint32_t));
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpData->m_base_address + XAM_MAX_PARALLEL_ITER_UPPER_OFFSET, &dfTmp[1], sizeof(uint32_t));


     retvalBuf[6] = dfTmp[0] << 32;

     retvalBuf[7] = dfTmp[1] << 32;

#if 0
      amResults.CuBusyCycles[index]      = dfTmp[0] << 32;
      amResults.CuMaxParallelIter[index] = dfTmp[1] << 32;
#endif
    }
  }

  for (int c=0; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++) {
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpData->m_base_address+am_offsets[c], &currData[c], sizeof(uint32_t));
  }
  retvalBuf[0] |= currData[0];

  retvalBuf[2] |= currData[1];

  retvalBuf[5] |= currData[2];

  retvalBuf[3] |= currData[3];

  retvalBuf[4] |= currData[4];

  retvalBuf[9] |= currData[5];

  retvalBuf[8] |= currData[6];

  retvalBuf[1] |= currData[7];
    

#if 0
  amResults.CuExecCount[index]      |= currData[0];
  amResults.CuExecCycles[index]     |= currData[1];
  amResults.CuStallExtCycles[index] |= currData[2];
  amResults.CuStallIntCycles[index] |= currData[3];
  amResults.CuStallStrCycles[index] |= currData[4];
  amResults.CuMinExecCycles[index]  |= currData[5];
  amResults.CuMaxExecCycles[index]  |= currData[6];
  amResults.CuStartCount[index]     |= currData[7];
#endif

  if(hasDataflow) {
    uint64_t dfTmp[2] = {0};
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpData->m_base_address + XAM_BUSY_CYCLES_OFFSET, &dfTmp[0], sizeof(uint32_t));
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpData->m_base_address + XAM_MAX_PARALLEL_ITER_OFFSET, &dfTmp[1], sizeof(uint32_t));

     retvalBuf[6] |= dfTmp[0] << 32;

     retvalBuf[7] |= dfTmp[1] << 32;

#if 0
    amResults.CuBusyCycles[index]      |= dfTmp[0] << 32;
    amResults.CuMaxParallelIter[index] |= dfTmp[1] << 32;
#endif

  } else {

     retvalBuf[6] = retvalBuf[8];

     retvalBuf[7] = 1;

#if 0
    amResults.CuBusyCycles[index]      = amResults.CuExecCycles[index];
    amResults.CuMaxParallelIter[index] = 1;
#endif
  }
  return retvalBuf;


}

} }
