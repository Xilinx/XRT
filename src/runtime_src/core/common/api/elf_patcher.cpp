// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_API_SOURCE         // in same dll as xrt_api
#define XCL_DRIVER_DLL_EXPORT  // in same dll as xrt_api
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "elf_patcher.h"
#include "core/common/error.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace xrt_core::elf_patcher {

// Return the minimum number of bytes required past bd_data_ptr for a given symbol type.
// Used to bounds-check the patch offset before any pointer arithmetic.
static size_t
required_patch_bytes(symbol_type type)
{
  switch (type) {
  case symbol_type::address_64:
    // words [0],[1]
    return 2 * sizeof(uint32_t);   // NOLINT(readability-magic-numbers)
  case symbol_type::scalar_32bit_kind:
    // word [0]
    return 1 * sizeof(uint32_t);   // NOLINT(readability-magic-numbers)
  case symbol_type::shim_dma_base_addr_symbol_kind:
    // words [1],[2],[8] — buffer must cover indices [0..8]
    return 9 * sizeof(uint32_t);   // NOLINT(readability-magic-numbers)
  case symbol_type::shim_dma_aie4_base_addr_symbol_kind:
    // words [0],[1]
    return 2 * sizeof(uint32_t);   // NOLINT(readability-magic-numbers)
  case symbol_type::control_packet_57:
  case symbol_type::control_packet_48:
    // words [2],[3] — buffer must cover indices [0..3]
    return 4 * sizeof(uint32_t);   // NOLINT(readability-magic-numbers)
  case symbol_type::shim_dma_48:
    // words [1],[2] — buffer must cover indices [0..2]
    return 3 * sizeof(uint32_t);   // NOLINT(readability-magic-numbers)
  case symbol_type::control_packet_57_aie4:
    // words [1],[2] — buffer must cover indices [0..2]
    return 3 * sizeof(uint32_t);   // NOLINT(readability-magic-numbers)
  case symbol_type::pl_ddr_64:
    // words [8],[9] — buffer must cover indices [0..9]
    return 10 * sizeof(uint32_t);  // NOLINT(readability-magic-numbers)
  default:
    // all the newly added symbols should be added in this function,
    // otherwise it will result in silent failure and potential memory corruption
    // so throw an exception to catch the issue early
    throw std::runtime_error("required_patch_bytes: unsupported symbol type");
  }
}

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

// patcher_config constructor - stores static configuration from ELF
patcher_config::
patcher_config(symbol_type type, std::vector<patch_config> configs, buf_type t)
  : m_symbol_type(type)
  , m_buf_type(t)
  , m_patch_configs(std::move(configs))
{}

void
patcher_config::
add_patch(const patch_config& pc)
{
  m_patch_configs.push_back(pc);
}

// symbol_patcher constructor
symbol_patcher::
symbol_patcher(const patcher_config* config)
  : m_config(config)
  , m_states(config ? config->m_patch_configs.size() : 0)
{
  if (m_config) {
    auto patch_words = required_patch_bytes(m_config->m_symbol_type) / sizeof(uint32_t);
    for (auto& state : m_states)
      state.bd_data_ptrs.resize(patch_words);
  }
}

void
symbol_patcher::
patch64(uint32_t* data_to_patch, uint64_t addr)
{
  *data_to_patch = static_cast<uint32_t>(addr & 0xffffffff);                // NOLINT
  *(data_to_patch + 1) = static_cast<uint32_t>((addr >> 32) & 0xffffffff);  // NOLINT
}

void
symbol_patcher::
patch32(uint32_t* data_to_patch, uint64_t register_value, uint32_t mask)
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
patch57(uint32_t* bd_data_ptr, uint64_t patch)
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
patch57_aie4(uint32_t* bd_data_ptr, uint64_t patch)
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
patch_ctrl57(uint32_t* bd_data_ptr, uint64_t patch)
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
patch_ctrl48(uint32_t* bd_data_ptr, uint64_t patch)
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
patch_shim48(uint32_t* bd_data_ptr, uint64_t patch)
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
patch_ctrl57_aie4(uint32_t* bd_data_ptr, uint64_t patch)
{
  // This patching scheme is originated from NPU firmware
  // bd_data_ptr is a pointer to the header of the control code
  uint64_t base_address = (((uint64_t)bd_data_ptr[1] & 0x1FFFFFF) << 32) | bd_data_ptr[2]; // NOLINT

  base_address += patch + get_ddr_aie_addr_offset();
  bd_data_ptr[2] = (uint32_t)(base_address & 0xFFFFFFFF);                                  // NOLINT
  bd_data_ptr[1] = (bd_data_ptr[1] & 0xFE000000) | ((base_address >> 32) & 0x1FFFFFF);     // NOLINT
}

void
symbol_patcher::
patch_pl_ddr64(uint32_t* bd_data_ptr, uint64_t patch)
{
  // PL kernel wts_params block: words [8] and [9] hold a plain 64-bit DDR address
  uint64_t base_address = (static_cast<uint64_t>(bd_data_ptr[9]) << 32) | bd_data_ptr[8]; // NOLINT
  base_address += patch;
  bd_data_ptr[8] = static_cast<uint32_t>(base_address & 0xFFFFFFFF);                      // NOLINT
  bd_data_ptr[9] = static_cast<uint32_t>((base_address >> 32) & 0xFFFFFFFF);              // NOLINT
}

void
symbol_patcher::
patch_symbol(xrt::bo bo, uint64_t value, bool first, bool is_arg)
{
  if (!m_config)
    throw std::runtime_error("symbol_patcher: config not set");

  auto base = reinterpret_cast<uint8_t*>(bo.map());
  auto bo_size = bo.size();
  const auto& configs = m_config->m_patch_configs;

  auto patch_bytes = required_patch_bytes(m_config->m_symbol_type);
  auto patch_words = patch_bytes / sizeof(uint32_t);

  // Ensure runtime state is properly sized; new entries need bd_data_ptrs initialized
  if (m_states.size() != configs.size()) {
    auto old_size = m_states.size();
    m_states.resize(configs.size());
    for (auto i = old_size; i < m_states.size(); ++i)
      m_states[i].bd_data_ptrs.resize(patch_words);
  }

  for (size_t i = 0; i < configs.size(); ++i) {
    const auto& config = configs[i];
    auto& state = m_states[i];

    auto offset = config.offset_to_patch_buffer;
    if (offset > bo_size || patch_bytes > bo_size - offset)
      throw xrt_core::error(-EINVAL, "ELF patch offset exceeds instruction buffer");

    auto bd_data_ptr = reinterpret_cast<uint32_t*>(base + offset);

    // If the symbol patched is an argument to kernel we have to cache
    // original bd data ptrs as args can be changed in subsequent runs.
    // For non-arg symbols we only patch and sync without caching
    // as they are patched once and never changed.
    if (is_arg) {
      if (state.bd_data_ptrs.size() != patch_words)
        throw xrt_core::error(-EINVAL, "patch_symbol : BD cache size mismatch");

      if (!state.dirty) {
        // first time patching: cache exact BD words needed for this symbol type
        std::copy(bd_data_ptr, bd_data_ptr + patch_words, state.bd_data_ptrs.begin());
        state.dirty = true;
      }
      else {
        // not the first time patching, restore cached BD words before re-patching
        std::copy(state.bd_data_ptrs.begin(), state.bd_data_ptrs.end(), bd_data_ptr);
      }
    }

    // Sync the patched words back to device.  patch_bytes bytes from offset covers
    // exactly the words touched by each patch function — same value used for
    // the bounds check above, so no size argument is needed here.
    auto sync = [&]() {
      bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, patch_bytes, offset);
    };

    switch (m_config->m_symbol_type) {
    case symbol_type::address_64:
      patch64(bd_data_ptr, value);
      if (!first)
        sync();
      break;
    case symbol_type::scalar_32bit_kind:
      if (config.mask) {
        patch32(bd_data_ptr, value, config.mask);
        if (!first)
          sync();
      }
      break;
    case symbol_type::shim_dma_base_addr_symbol_kind:
      patch57(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first)
        sync();
      break;
    case symbol_type::shim_dma_aie4_base_addr_symbol_kind:
      patch57_aie4(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first)
        sync();
      break;
    case symbol_type::control_packet_57:
      patch_ctrl57(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first)
        sync();
      break;
    case symbol_type::control_packet_48:
      patch_ctrl48(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first)
        sync();
      break;
    case symbol_type::shim_dma_48:
      patch_shim48(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first)
        sync();
      break;
    case symbol_type::control_packet_57_aie4:
      patch_ctrl57_aie4(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first)
        sync();
      break;
    case symbol_type::pl_ddr_64:
      // words [8]+[9]: sync only those 2 words at their position within the block;
      // the bounds check above (patch_bytes= 10 words) already covers this sub-range.
      patch_pl_ddr64(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first)
        bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, 2 * sizeof(uint32_t), offset + 8 * sizeof(uint32_t)); // NOLINT
      break;
    default:
      throw std::runtime_error("Unsupported symbol type");
    }
  }
}

void
symbol_patcher::
patch_symbol_raw(uint8_t* base, size_t buf_size, uint64_t value, const patcher_config& config)
{
  for (const auto& cfg : config.m_patch_configs) {
    auto offset = cfg.offset_to_patch_buffer;
    auto patch_bytes = required_patch_bytes(config.m_symbol_type);
    if (offset > buf_size || patch_bytes > buf_size - offset)
      throw xrt_core::error(-EINVAL, "ELF patch offset exceeds instruction buffer");
    auto bd_data_ptr = reinterpret_cast<uint32_t*>(base + offset); // NOLINT

    switch (config.m_symbol_type) {
    case symbol_type::address_64:
      patch64(bd_data_ptr, value);
      break;
    case symbol_type::scalar_32bit_kind:
      if (cfg.mask)
        patch32(bd_data_ptr, value, cfg.mask);
      break;
    case symbol_type::shim_dma_base_addr_symbol_kind:
      patch57(bd_data_ptr, value + cfg.offset_to_base_bo_addr);
      break;
    case symbol_type::shim_dma_aie4_base_addr_symbol_kind:
        patch57_aie4(bd_data_ptr, value + cfg.offset_to_base_bo_addr);
      break;
    case symbol_type::control_packet_57:
      patch_ctrl57(bd_data_ptr, value + cfg.offset_to_base_bo_addr);
      break;
    case symbol_type::control_packet_48:
      patch_ctrl48(bd_data_ptr, value + cfg.offset_to_base_bo_addr);
      break;
    case symbol_type::shim_dma_48:
      patch_shim48(bd_data_ptr, value + cfg.offset_to_base_bo_addr);
      break;
    case symbol_type::control_packet_57_aie4:
      patch_ctrl57_aie4(bd_data_ptr, value + cfg.offset_to_base_bo_addr);
      break;
    case symbol_type::pl_ddr_64:
      patch_pl_ddr64(bd_data_ptr, value + cfg.offset_to_base_bo_addr);
      break;
    default:
      throw std::runtime_error("Unsupported symbol type");
    }
  }
}

} // namespace xrt_core::elf_patcher
