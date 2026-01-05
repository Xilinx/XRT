// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "elf_patcher.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace xrt_core::elf_patcher {

// Get AIE DDR address offset
inline static uint64_t
get_ddr_aie_addr_offset()
{
  constexpr uint64_t ddr_aie_addr_offset = 0x80000000;
#ifndef _WIN32
  // On NPU3 emulation platform
  // there is no DDR offset needed for AIE shim tile
  static const char* xemtarget = std::getenv("XCL_EMULATION_DEVICE_TARGET"); // NOLINT
  static const bool is_npu3_snl = xemtarget && (std::strcmp(xemtarget, "npu3_snl") == 0);
  if (is_npu3_snl)
    return 0;
#endif
  return ddr_aie_addr_offset;
}

symbol_patcher::
symbol_patcher(symbol_type type, std::vector<patch_info> patch_infos, buf_type t)
  : m_buf_type(t)
  , m_symbol_type(type)
  , m_patch_infos(std::move(patch_infos))
{}

void
symbol_patcher::
patch_symbol(uint8_t* base, uint64_t value) const
{
  // this function is used by internal shim level tests
  // which does explicit sync of buffers
  // so needn't do a partial sync
  patch_symbol_helper(base, value, true /*no partial sync*/);
}

void
symbol_patcher::
patch_symbol(xrt::bo bo, uint64_t value, bool first) const
{
  patch_symbol_helper(bo, value, first);
}

void
symbol_patcher::
patch64(uint32_t* data_to_patch, uint64_t addr) const
{
  *data_to_patch = static_cast<uint32_t>(addr & 0xffffffff);
  *(data_to_patch + 1) = static_cast<uint32_t>((addr >> 32) & 0xffffffff);
}

void
symbol_patcher::
patch32(uint32_t* data_to_patch, uint64_t register_value, uint32_t mask) const
{
  // Replace certain bits of *data_to_patch with register_value. Which bits to be replaced is specified by mask
  // For     *data_to_patch be 0xbb11aaaa and mask be 0x00ff0000
  // To make *data_to_patch be 0xbb55aaaa, register_value must be 0x00550000
  if ((reinterpret_cast<uintptr_t>(data_to_patch) & 0x3) != 0)
    throw std::runtime_error("address is not 4 byte aligned for patch32");

  auto new_value = *data_to_patch;
  new_value = (new_value & ~mask) | (register_value & mask);
  *data_to_patch = new_value;
}

void
symbol_patcher::
patch57(uint32_t* bd_data_ptr, uint64_t patch) const
{
  uint64_t base_address =
    ((static_cast<uint64_t>(bd_data_ptr[8]) & 0x1FF) << 48) |                       // NOLINT
    ((static_cast<uint64_t>(bd_data_ptr[2]) & 0xFFFF) << 32) |                      // NOLINT
    bd_data_ptr[1];

  base_address += patch;
  bd_data_ptr[1] = (uint32_t)(base_address & 0xFFFFFFFF);                           // NOLINT
  bd_data_ptr[2] = (bd_data_ptr[2] & 0xFFFF0000) | ((base_address >> 32) & 0xFFFF); // NOLINT
  bd_data_ptr[8] = (bd_data_ptr[8] & 0xFFFFFE00) | ((base_address >> 48) & 0x1FF);  // NOLINT
}

void
symbol_patcher::
patch57_aie4(uint32_t* bd_data_ptr, uint64_t patch) const
{
  uint64_t base_address =
    ((static_cast<uint64_t>(bd_data_ptr[0]) & 0x1FFFFFF) << 32) |                   // NOLINT
    bd_data_ptr[1];

  base_address += patch + get_ddr_aie_addr_offset();
  bd_data_ptr[1] = (uint32_t)(base_address & 0xFFFFFFFF);                           // NOLINT
  bd_data_ptr[0] = (bd_data_ptr[0] & 0xFE000000) | ((base_address >> 32) & 0x1FFFFFF);// NOLINT
}

void
symbol_patcher::
patch_ctrl57(uint32_t* bd_data_ptr, uint64_t patch) const
{
  //TODO need to change below logic to patch 57 bits
  uint64_t base_address =
    ((static_cast<uint64_t>(bd_data_ptr[3]) & 0xFFF) << 32) |                       // NOLINT
    ((static_cast<uint64_t>(bd_data_ptr[2])));

  base_address = base_address + patch;
  bd_data_ptr[2] = (uint32_t)(base_address & 0xFFFFFFFC);                           // NOLINT
  bd_data_ptr[3] = (bd_data_ptr[3] & 0xFFFF0000) | (base_address >> 32);            // NOLINT
}

void
symbol_patcher::
patch_ctrl48(uint32_t* bd_data_ptr, uint64_t patch) const
{
  // This patching scheme is originated from NPU firmware
  uint64_t base_address =
    ((static_cast<uint64_t>(bd_data_ptr[3]) & 0xFFF) << 32) |                       // NOLINT
    ((static_cast<uint64_t>(bd_data_ptr[2])));

  base_address = base_address + patch + get_ddr_aie_addr_offset();
  bd_data_ptr[2] = (uint32_t)(base_address & 0xFFFFFFFC);                           // NOLINT
  bd_data_ptr[3] = (bd_data_ptr[3] & 0xFFFF0000) | (base_address >> 32);            // NOLINT
}

void
symbol_patcher::
patch_shim48(uint32_t* bd_data_ptr, uint64_t patch) const
{
  // This patching scheme is originated from NPU firmware
  uint64_t base_address =
    ((static_cast<uint64_t>(bd_data_ptr[2]) & 0xFFFF) << 32) |                      // NOLINT
    ((static_cast<uint64_t>(bd_data_ptr[1])));

  base_address = base_address + patch + get_ddr_aie_addr_offset();
  bd_data_ptr[1] = (uint32_t)(base_address & 0xFFFFFFFC);                           // NOLINT
  bd_data_ptr[2] = (bd_data_ptr[2] & 0xFFFF0000) | (base_address >> 32);            // NOLINT
}

void
symbol_patcher::
patch_ctrl57_aie4(uint32_t* bd_data_ptr, uint64_t patch) const
{
  // This patching scheme is originated from NPU firmware
  // bd_data_ptr is a pointer to the header of the control code
  uint64_t base_address = (((uint64_t)bd_data_ptr[1] & 0x1FFFFFF) << 32) | bd_data_ptr[2]; // NOLINT

  base_address += patch + get_ddr_aie_addr_offset();
  bd_data_ptr[2] = (uint32_t)(base_address & 0xFFFFFFFF);                                  // NOLINT
  bd_data_ptr[1] = (bd_data_ptr[1] & 0xFE000000) | ((base_address >> 32) & 0x1FFFFFF);     // NOLINT
}

} // namespace xrt_core::elf_patcher
