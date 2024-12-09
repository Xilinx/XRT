/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include "core/common/device.h"
#include "core/include/xrt/detail/xclbin.h"
#include "core/include/xdp/add.h"
#include "core/include/xdp/aim.h"
#include "core/include/xdp/am.h"
#include "core/include/xdp/asm.h"
#include "core/include/xdp/app_debug.h"
#include "core/include/xdp/lapc.h"
#include "core/include/xdp/spc.h"

namespace xrt_core { namespace debug_ip {

// Read AIM counter values using "xread" for Edge and Windows PCIe 
std::vector<uint64_t> 
get_aim_counter_result(const xrt_core::device* device, debug_ip_data* dbg_ip_data)
{
  // read counter values
  static const uint64_t aim_offsets[] = {
    xdp::IP::AIM::AXI_LITE::WRITE_BYTES,
    xdp::IP::AIM::AXI_LITE::WRITE_TRANX,
    xdp::IP::AIM::AXI_LITE::READ_BYTES,
    xdp::IP::AIM::AXI_LITE::READ_TRANX,
    xdp::IP::AIM::AXI_LITE::OUTSTANDING_COUNTS,
    xdp::IP::AIM::AXI_LITE::LAST_WRITE_ADDRESS,
    xdp::IP::AIM::AXI_LITE::LAST_WRITE_DATA,
    xdp::IP::AIM::AXI_LITE::LAST_READ_ADDRESS,
    xdp::IP::AIM::AXI_LITE::LAST_READ_DATA,
  };

  static const uint64_t aim_upper_offsets[] = {
    xdp::IP::AIM::AXI_LITE::WRITE_BYTES_UPPER,
    xdp::IP::AIM::AXI_LITE::WRITE_TRANX_UPPER,
    xdp::IP::AIM::AXI_LITE::READ_BYTES_UPPER,
    xdp::IP::AIM::AXI_LITE::READ_TRANX_UPPER,
    xdp::IP::AIM::AXI_LITE::OUTSTANDING_COUNTS_UPPER,
    xdp::IP::AIM::AXI_LITE::LAST_WRITE_ADDRESS_UPPER,
    xdp::IP::AIM::AXI_LITE::LAST_WRITE_DATA_UPPER,
    xdp::IP::AIM::AXI_LITE::LAST_READ_ADDRESS_UPPER,
    xdp::IP::AIM::AXI_LITE::LAST_READ_DATA_UPPER,
  };

  std::vector<uint64_t> ret_val(xdp::IP::AIM::NUM_COUNTERS_REPORT);

  uint32_t curr_data[xdp::IP::AIM::NUM_COUNTERS_REPORT] = {0};

  uint32_t sample_interval = 0;

  // Read sample interval register to latch the sampled metric counters
  device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, 
                dbg_ip_data->m_base_address + xdp::IP::AIM::AXI_LITE::SAMPLE,
                &sample_interval, sizeof(uint32_t));

  // If applicable, read the upper 32-bits of the 64-bit debug counters
  if (dbg_ip_data->m_properties & xdp::IP::AIM::mask::PROPERTY_64BIT) {
    for (int c = 0; c < xdp::IP::AIM::NUM_COUNTERS_REPORT; c++) {
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    dbg_ip_data->m_base_address + aim_upper_offsets[c],
                    &curr_data[c], sizeof(uint32_t));
      ret_val[c] = (static_cast<uint64_t>(curr_data[c])) << 32;
    }
  }

  for (int c = 0; c < xdp::IP::AIM::NUM_COUNTERS_REPORT; c++) {
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                  dbg_ip_data->m_base_address + aim_offsets[c],
                  &curr_data[c], sizeof(uint32_t));
    ret_val[c] |= curr_data[c];
  }

  return ret_val;

}

// Read AM counter values using "xread" for Edge and Windows PCIe 
std::vector<uint64_t> 
get_am_counter_result(const xrt_core::device* device, debug_ip_data* dbg_ip_data)
{
  // read counter values
  // These are in "xbutil" order
  static const uint64_t am_offsets[] = {
    xdp::IP::AM::AXI_LITE::EXECUTION_COUNT,
    xdp::IP::AM::AXI_LITE::EXECUTION_CYCLES,
    xdp::IP::AM::AXI_LITE::STALL_INT,
    xdp::IP::AM::AXI_LITE::STALL_STR,
    xdp::IP::AM::AXI_LITE::STALL_EXT,
    xdp::IP::AM::AXI_LITE::MIN_EXECUTION_CYCLES,
    xdp::IP::AM::AXI_LITE::MAX_EXECUTION_CYCLES,
    xdp::IP::AM::AXI_LITE::TOTAL_CU_START
  };

  // These are in "xbutil" order
  static const uint64_t am_upper_offsets[] = {
    xdp::IP::AM::AXI_LITE::EXECUTION_COUNT_UPPER,
    xdp::IP::AM::AXI_LITE::EXECUTION_CYCLES_UPPER,
    xdp::IP::AM::AXI_LITE::STALL_INT_UPPER,
    xdp::IP::AM::AXI_LITE::STALL_STR_UPPER,
    xdp::IP::AM::AXI_LITE::STALL_EXT_UPPER,
    xdp::IP::AM::AXI_LITE::MIN_EXECUTION_CYCLES_UPPER,
    xdp::IP::AM::AXI_LITE::MAX_EXECUTION_CYCLES_UPPER,
    xdp::IP::AM::AXI_LITE::TOTAL_CU_START_UPPER
  };

  // We're returning all the registers as if we were reading from sysfs,
  // but we're only guaranteed the non-dataflow values.
  std::vector<uint64_t> ret_val(xdp::IP::AM::NUM_COUNTERS);

  // Read all metric counters
  uint32_t curr_data[xdp::IP::AM::NUM_COUNTERS_REPORT] = {0};

  uint32_t sample_interval = 0;
  // Read sample interval register to latch the sampled metric counters
  device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                dbg_ip_data->m_base_address + xdp::IP::AM::AXI_LITE::SAMPLE,
                &sample_interval, sizeof(uint32_t));

  auto dbg_ip_version = std::make_pair(dbg_ip_data->m_major, dbg_ip_data->m_minor);

  std::pair<uint8_t, uint8_t> ref_version { static_cast<uint8_t>(1), static_cast<uint8_t>(1) };

  bool has_dataflow = (dbg_ip_version > ref_version);

  // If applicable, read the upper 32-bits of the 64-bit debug counters
  if (dbg_ip_data->m_properties & xdp::IP::AIM::mask::PROPERTY_64BIT) {
    for (int c = 0; c < xdp::IP::AM::NUM_COUNTERS_REPORT; c++) {
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    dbg_ip_data->m_base_address + am_upper_offsets[c],
                    &curr_data[c], sizeof(uint32_t));
    }

    auto get_upper_bytes = [&] (auto dest, auto src) { ret_val[dest] = (static_cast<uint64_t>(curr_data[src])) << 32; };


    get_upper_bytes(xdp::IP::AM::sysfs::EXECUTION_COUNT,
                    xdp::IP::AM::report::EXECUTION_COUNT);
    get_upper_bytes(xdp::IP::AM::sysfs::EXECUTION_CYCLES,
                    xdp::IP::AM::report::EXECUTION_CYCLES);
    get_upper_bytes(xdp::IP::AM::sysfs::STALL_INT,
                    xdp::IP::AM::report::STALL_INT);
    get_upper_bytes(xdp::IP::AM::sysfs::STALL_STR,
                    xdp::IP::AM::report::STALL_STR);
    get_upper_bytes(xdp::IP::AM::sysfs::STALL_EXT,
                    xdp::IP::AM::report::STALL_EXT);
    get_upper_bytes(xdp::IP::AM::sysfs::MIN_EXECUTION_CYCLES,
                    xdp::IP::AM::report::MIN_EXECUTION_CYCLES);
    get_upper_bytes(xdp::IP::AM::sysfs::MAX_EXECUTION_CYCLES,
                    xdp::IP::AM::report::MAX_EXECUTION_CYCLES);
    get_upper_bytes(xdp::IP::AM::sysfs::TOTAL_CU_START,
                    xdp::IP::AM::report::TOTAL_CU_START);

    if (has_dataflow) {
      uint64_t busy_cycles = 0, max_parallel = 0;
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbg_ip_data->m_base_address + xdp::IP::AM::AXI_LITE::BUSY_CYCLES_UPPER, &busy_cycles, sizeof(uint32_t));
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbg_ip_data->m_base_address + xdp::IP::AM::AXI_LITE::MAX_PARALLEL_ITER_UPPER, &max_parallel, sizeof(uint32_t));

      ret_val[xdp::IP::AM::sysfs::BUSY_CYCLES] = busy_cycles << 32;
      ret_val[xdp::IP::AM::sysfs::MAX_PARALLEL_ITER] = max_parallel << 32;
    }
  }

  // Read the lower 32-bits and add it to the final return value
  for (int c = 0; c < xdp::IP::AM::NUM_COUNTERS_REPORT; c++) {
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbg_ip_data->m_base_address+am_offsets[c], &curr_data[c], sizeof(uint32_t));
  }

  auto get_lower_bytes = [&] (auto dest, auto src) { ret_val[dest] |= curr_data[src]; };

  get_lower_bytes(xdp::IP::AM::sysfs::EXECUTION_COUNT,
                  xdp::IP::AM::report::EXECUTION_COUNT);
  get_lower_bytes(xdp::IP::AM::sysfs::EXECUTION_CYCLES,
                  xdp::IP::AM::report::EXECUTION_CYCLES);
  get_lower_bytes(xdp::IP::AM::sysfs::STALL_INT,
                  xdp::IP::AM::report::STALL_INT);
  get_lower_bytes(xdp::IP::AM::sysfs::STALL_STR,
                  xdp::IP::AM::report::STALL_STR);
  get_lower_bytes(xdp::IP::AM::sysfs::STALL_EXT,
                  xdp::IP::AM::report::STALL_EXT);
  get_lower_bytes(xdp::IP::AM::sysfs::MIN_EXECUTION_CYCLES,
                  xdp::IP::AM::report::MIN_EXECUTION_CYCLES);
  get_lower_bytes(xdp::IP::AM::sysfs::MAX_EXECUTION_CYCLES,
                  xdp::IP::AM::report::MAX_EXECUTION_CYCLES);
  get_lower_bytes(xdp::IP::AM::sysfs::TOTAL_CU_START,
                  xdp::IP::AM::report::TOTAL_CU_START);

  if (has_dataflow) {
    uint64_t busy_cycles = 0, max_parallel = 0;
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbg_ip_data->m_base_address + xdp::IP::AM::AXI_LITE::BUSY_CYCLES, &busy_cycles, sizeof(uint32_t));
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbg_ip_data->m_base_address + xdp::IP::AM::AXI_LITE::MAX_PARALLEL_ITER, &max_parallel, sizeof(uint32_t));

    ret_val[xdp::IP::AM::sysfs::BUSY_CYCLES] |= busy_cycles;
    ret_val[xdp::IP::AM::sysfs::MAX_PARALLEL_ITER] |= max_parallel;
  } else {
    ret_val[xdp::IP::AM::sysfs::BUSY_CYCLES] = ret_val[xdp::IP::AM::sysfs::MAX_EXECUTION_CYCLES];
    ret_val[xdp::IP::AM::sysfs::MAX_PARALLEL_ITER] = 1;
  }
  return ret_val;

}

// Read ASM counter values using "xread" for Edge and Windows PCIe 
std::vector<uint64_t> 
get_asm_counter_result(const xrt_core::device* device, debug_ip_data* dbg_ip_data)
{
  static const uint64_t asm_offsets[] = {
    xdp::IP::ASM::AXI_LITE::NUM_TRANX,
    xdp::IP::ASM::AXI_LITE::DATA_BYTES,
    xdp::IP::ASM::AXI_LITE::BUSY_CYCLES,
    xdp::IP::ASM::AXI_LITE::STALL_CYCLES,
    xdp::IP::ASM::AXI_LITE::STARVE_CYCLES
  };

  std::vector<uint64_t> ret_val(xdp::IP::ASM::NUM_COUNTERS);

  uint32_t sample_interval = 0 ;
  // Read sample interval register to latch the sampled metric counters
  device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                dbg_ip_data->m_base_address + xdp::IP::ASM::AXI_LITE::SAMPLE,
                &sample_interval, sizeof(uint32_t));

  // Then read all the individual 64-bit counters
  for (unsigned int j = 0 ; j < xdp::IP::ASM::NUM_COUNTERS; j++) {
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                  dbg_ip_data->m_base_address + asm_offsets[j],
                  &ret_val[j], sizeof(uint64_t));
  }

  return ret_val;

}

// Read LAPC status using "xread" for Edge and Windows PCIe 
std::vector<uint32_t> 
get_lapc_status(const xrt_core::device* device, debug_ip_data* dbg_ip_data)
{
  static const uint64_t statusRegisters[] = {
    xdp::IP::LAPC::AXI_LITE::STATUS,
    xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_0,
    xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_1,
    xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_2,
    xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_3,
    xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_0,
    xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_1,
    xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_2,
    xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_3
  };

  std::vector<uint32_t> ret_val(xdp::IP::LAPC::NUM_COUNTERS);

  for (int c = 0; c < xdp::IP::LAPC::NUM_COUNTERS; c++) {
    device->xread(XCL_ADDR_SPACE_DEVICE_CHECKER, 
                  dbg_ip_data->m_base_address+statusRegisters[c], 
                  &ret_val[c], sizeof(uint32_t));
  }

  return ret_val;
}

// Read SPC status using "xread" for Edge and Windows PCIe 
std::vector<uint32_t> 
get_spc_status(const xrt_core::device* device, debug_ip_data* dbg_ip_data)
{
  std::vector<uint32_t> ret_val(xdp::IP::SPC::NUM_COUNTERS);

  device->xread(XCL_ADDR_SPACE_DEVICE_CHECKER,
                dbg_ip_data->m_base_address + xdp::IP::SPC::AXI_LITE::PC_ASSERTED,
                &ret_val[xdp::IP::SPC::sysfs::PC_ASSERTED], sizeof(uint32_t));
  device->xread(XCL_ADDR_SPACE_DEVICE_CHECKER,
                dbg_ip_data->m_base_address + xdp::IP::SPC::AXI_LITE::CURRENT_PC,
                &ret_val[xdp::IP::SPC::sysfs::CURRENT_PC], sizeof(uint32_t));
  device->xread(XCL_ADDR_SPACE_DEVICE_CHECKER,
                dbg_ip_data->m_base_address + xdp::IP::SPC::AXI_LITE::SNAPSHOT_PC,
                &ret_val[xdp::IP::SPC::sysfs::SNAPSHOT_PC], sizeof(uint32_t));

  return ret_val;
}

// Read Accelerator Deadlock Detector Status values using "xread" for Edge and Windows PCIe 
uint32_t 
get_accel_deadlock_status(const xrt_core::device* device, debug_ip_data* dbg_ip_data)
{
  uint32_t ret_val = 0;

  device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                dbg_ip_data->m_base_address + xdp::IP::ADD::AXI_LITE::STATUS,
                &ret_val, sizeof(ret_val));

  return ret_val;
}


} }
