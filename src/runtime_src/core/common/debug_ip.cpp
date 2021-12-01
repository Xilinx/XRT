/**
 * Copyright (C) 2021 Xilinx, Inc
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
#include "core/include/xclbin.h"
#include "core/include/xcl_perfmon_parameters.h"

namespace xrt_core { namespace debug_ip {

// Read AIM counter values using "xread" for Edge and Windows PCIe 
std::vector<uint64_t> 
get_aim_counter_result(const xrt_core::device* device, debug_ip_data* dbg_ip_data)
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

  std::vector<uint64_t> ret_val(XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT);

  uint32_t curr_data[XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT] = {0};

  uint32_t sample_interval = 0;

  // Read sample interval register to latch the sampled metric counters
  device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, 
                dbg_ip_data->m_base_address + XAIM_SAMPLE_OFFSET,
                &sample_interval, sizeof(uint32_t));

  // If applicable, read the upper 32-bits of the 64-bit debug counters
  if (dbg_ip_data->m_properties & XAIM_64BIT_PROPERTY_MASK) {
    for (int c = 0 ; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; c++) {
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    dbg_ip_data->m_base_address + aim_upper_offsets[c],
                    &curr_data[c], sizeof(uint32_t));
      ret_val[c] = (static_cast<uint64_t>(curr_data[c])) << 32;
    }
  }

  for (int c = 0; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++) {
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

  std::vector<uint64_t> ret_val(XAM_TOTAL_DEBUG_COUNTERS_PER_SLOT);

  // Read all metric counters
  uint32_t curr_data[XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT] = {0};

  uint32_t sample_interval = 0;
  // Read sample interval register to latch the sampled metric counters
  device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                dbg_ip_data->m_base_address + XAM_SAMPLE_OFFSET,
                &sample_interval, sizeof(uint32_t));

  auto dbg_ip_version = std::make_pair(dbg_ip_data->m_major, dbg_ip_data->m_minor);

  std::pair<uint8_t, uint8_t> ref_version { 1, 1};

  bool has_dataflow = (dbg_ip_version > ref_version);

  // If applicable, read the upper 32-bits of the 64-bit debug counters
  if (dbg_ip_data->m_properties & XAM_64BIT_PROPERTY_MASK) {
    for (int c = 0 ; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; c++) {
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    dbg_ip_data->m_base_address + am_upper_offsets[c],
                    &curr_data[c], sizeof(uint32_t));
    }

    auto get_upper_bytes = [&] (auto dest, auto src) { ret_val[dest] = (static_cast<uint64_t>(curr_data[src])) << 32; };

    get_upper_bytes(XAM_IOCTL_EXECUTION_COUNT_INDEX, XAM_ACCEL_EXECUTION_COUNT_INDEX);
    get_upper_bytes(XAM_IOCTL_EXECUTION_CYCLES_INDEX, XAM_ACCEL_EXECUTION_CYCLES_INDEX);
    get_upper_bytes(XAM_IOCTL_STALL_INT_INDEX, XAM_ACCEL_STALL_INT_INDEX);
    get_upper_bytes(XAM_IOCTL_STALL_STR_INDEX, XAM_ACCEL_STALL_STR_INDEX);
    get_upper_bytes(XAM_IOCTL_STALL_EXT_INDEX, XAM_ACCEL_STALL_EXT_INDEX);
    get_upper_bytes(XAM_IOCTL_MIN_EXECUTION_CYCLES_INDEX, XAM_ACCEL_MIN_EXECUTION_CYCLES_INDEX);
    get_upper_bytes(XAM_IOCTL_MAX_EXECUTION_CYCLES_INDEX, XAM_ACCEL_MAX_EXECUTION_CYCLES_INDEX);
    get_upper_bytes(XAM_IOCTL_START_COUNT_INDEX, XAM_ACCEL_TOTAL_CU_START_INDEX);
    
    if (has_dataflow) {
      uint64_t busy_cycles = 0, max_parallel = 0;
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbg_ip_data->m_base_address + XAM_BUSY_CYCLES_UPPER_OFFSET, &busy_cycles, sizeof(uint32_t));
      device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbg_ip_data->m_base_address + XAM_MAX_PARALLEL_ITER_UPPER_OFFSET, &max_parallel, sizeof(uint32_t));

      ret_val[XAM_IOCTL_BUSY_CYCLES_INDEX] = busy_cycles << 32;
      ret_val[XAM_IOCTL_MAX_PARALLEL_ITR_INDEX] = max_parallel << 32;

    }
  }

  for (int c = 0; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++) {
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbg_ip_data->m_base_address+am_offsets[c], &curr_data[c], sizeof(uint32_t));
  }

  auto get_lower_bytes = [&] (auto dest, auto src) { ret_val[dest] |= curr_data[src]; };

  get_lower_bytes(XAM_IOCTL_EXECUTION_COUNT_INDEX, XAM_ACCEL_EXECUTION_COUNT_INDEX);
  get_lower_bytes(XAM_IOCTL_EXECUTION_CYCLES_INDEX, XAM_ACCEL_EXECUTION_CYCLES_INDEX);
  get_lower_bytes(XAM_IOCTL_STALL_INT_INDEX, XAM_ACCEL_STALL_INT_INDEX);
  get_lower_bytes(XAM_IOCTL_STALL_STR_INDEX, XAM_ACCEL_STALL_STR_INDEX);
  get_lower_bytes(XAM_IOCTL_STALL_EXT_INDEX, XAM_ACCEL_STALL_EXT_INDEX);
  get_lower_bytes(XAM_IOCTL_MIN_EXECUTION_CYCLES_INDEX, XAM_ACCEL_MIN_EXECUTION_CYCLES_INDEX);
  get_lower_bytes(XAM_IOCTL_MAX_EXECUTION_CYCLES_INDEX, XAM_ACCEL_MAX_EXECUTION_CYCLES_INDEX);
  get_lower_bytes(XAM_IOCTL_START_COUNT_INDEX, XAM_ACCEL_TOTAL_CU_START_INDEX);

  if (has_dataflow) {
    uint64_t busy_cycles = 0, max_parallel = 0;
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbg_ip_data->m_base_address + XAM_BUSY_CYCLES_OFFSET, &busy_cycles, sizeof(uint32_t));
    device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON, dbg_ip_data->m_base_address + XAM_MAX_PARALLEL_ITER_OFFSET, &max_parallel, sizeof(uint32_t));

    ret_val[XAM_IOCTL_BUSY_CYCLES_INDEX] |= busy_cycles;
    ret_val[XAM_IOCTL_MAX_PARALLEL_ITR_INDEX] |= max_parallel;
  } else {
    ret_val[XAM_IOCTL_BUSY_CYCLES_INDEX] = ret_val[XAM_IOCTL_MAX_EXECUTION_CYCLES_INDEX];
    ret_val[XAM_IOCTL_MAX_PARALLEL_ITR_INDEX] = 1;
  }
  return ret_val;

}

// Read ASM counter values using "xread" for Edge and Windows PCIe 
std::vector<uint64_t> 
get_asm_counter_result(const xrt_core::device* device, debug_ip_data* dbg_ip_data)
{
  static const uint64_t asm_offsets[] = {
    XASM_NUM_TRANX_OFFSET,
    XASM_DATA_BYTES_OFFSET,
    XASM_BUSY_CYCLES_OFFSET,
    XASM_STALL_CYCLES_OFFSET,
    XASM_STARVE_CYCLES_OFFSET
  };

  std::vector<uint64_t> ret_val(XASM_DEBUG_SAMPLE_COUNTERS_PER_SLOT);

  uint32_t sample_interval = 0 ;
  // Read sample interval register to latch the sampled metric counters
  device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                dbg_ip_data->m_base_address + XASM_SAMPLE_OFFSET,
                &sample_interval, sizeof(uint32_t));

  // Then read all the individual 64-bit counters
  for (unsigned int j = 0 ; j < XASM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; j++) {
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
    LAPC_OVERALL_STATUS_OFFSET,

    LAPC_CUMULATIVE_STATUS_0_OFFSET, LAPC_CUMULATIVE_STATUS_1_OFFSET,
    LAPC_CUMULATIVE_STATUS_2_OFFSET, LAPC_CUMULATIVE_STATUS_3_OFFSET,

    LAPC_SNAPSHOT_STATUS_0_OFFSET, LAPC_SNAPSHOT_STATUS_1_OFFSET,
    LAPC_SNAPSHOT_STATUS_2_OFFSET, LAPC_SNAPSHOT_STATUS_3_OFFSET
  };

  std::vector<uint32_t> ret_val(XLAPC_STATUS_PER_SLOT);

  for (int c = 0; c < XLAPC_STATUS_PER_SLOT; c++) {
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
  std::vector<uint32_t> ret_val(XSPC_STATUS_PER_SLOT);

  device->xread(XCL_ADDR_SPACE_DEVICE_CHECKER,
                dbg_ip_data->m_base_address + XSPC_PC_ASSERTED_OFFSET,
                &ret_val[XSPC_PC_ASSERTED], sizeof(uint32_t));
  device->xread(XCL_ADDR_SPACE_DEVICE_CHECKER,
                dbg_ip_data->m_base_address + XSPC_CURRENT_PC_OFFSET,
                &ret_val[XSPC_CURRENT_PC], sizeof(uint32_t));
  device->xread(XCL_ADDR_SPACE_DEVICE_CHECKER,
                dbg_ip_data->m_base_address + XSPC_SNAPSHOT_PC_OFFSET,
                &ret_val[XSPC_SNAPSHOT_PC], sizeof(uint32_t));

  return ret_val;
}

// Read Accelerator Deadlock Detector Status values using "xread" for Edge and Windows PCIe 
uint32_t 
get_accel_deadlock_status(const xrt_core::device* device, debug_ip_data* dbg_ip_data)
{
  uint32_t ret_val = 0;

  device->xread(XCL_ADDR_SPACE_DEVICE_PERFMON,
                dbg_ip_data->m_base_address + XACCEL_DEADLOCK_STATUS_OFFSET,
                &ret_val, sizeof(ret_val));

  return ret_val;
}


} }
