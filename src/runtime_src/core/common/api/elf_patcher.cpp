// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_API_SOURCE         // in same dll as xrt_api
#define XCL_DRIVER_DLL_EXPORT  // in same dll as xrt_api
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
{}

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
patch_symbol(xrt::bo bo, uint64_t value, bool first)
{
  if (!m_config)
    throw std::runtime_error("symbol_patcher: config not set");

  auto base = reinterpret_cast<uint8_t*>(bo.map());
  const auto& configs = m_config->m_patch_configs;

  // Ensure runtime state is properly sized
  if (m_states.size() != configs.size())
    m_states.resize(configs.size());

  for (size_t i = 0; i < configs.size(); ++i) {
    const auto& config = configs[i];
    auto& state = m_states[i];

    auto offset = config.offset_to_patch_buffer;
    auto bd_data_ptr = reinterpret_cast<uint32_t*>(base + offset);

    if (!state.dirty) {
      // first time patching cache bd ptr values using bd ptrs array in patch state
      std::copy(bd_data_ptr, bd_data_ptr + max_bd_words, state.bd_data_ptrs.begin());
      state.dirty = true;
    }
    else {
      // not the first time patching, restore bd ptr values from patch state bd ptrs array
      std::copy(state.bd_data_ptrs.begin(), state.bd_data_ptrs.end(), bd_data_ptr);
    }

    // lambda for calling sync bo
    // We only sync the words that are patched not the entire bo
    auto sync = [&](size_t size) {
      bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, size, offset);
    };

    switch (m_config->m_symbol_type) {
    case symbol_type::address_64:
      // value is a 64bit address
      patch64(bd_data_ptr, value);
      if (!first) {
        // sync 64 bits patched
        sync(sizeof(uint64_t));
      }
      break;
    case symbol_type::scalar_32bit_kind:
      // value is a register value
      if (config.mask) {
        patch32(bd_data_ptr, value, config.mask);
        if (!first) {
          // sync 32 bits patched
          sync(sizeof(uint32_t));
        }
      }
      break;
    case symbol_type::shim_dma_base_addr_symbol_kind:
      // value is a bo address
      patch57(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first) {
        // sync all the words (max_bd_words)
        sync(sizeof(uint32_t) * max_bd_words);
      }
      break;
    case symbol_type::shim_dma_aie4_base_addr_symbol_kind:
      // value is a bo address
      patch57_aie4(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first) {
        // sync 64 bits or 2 words
        sync(sizeof(uint64_t));
      }
      break;
    case symbol_type::control_packet_57:
      // value is a bo address
      patch_ctrl57(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first) {
        // Data in this case is written till 3rd offset of bd_data_ptr
        // so syncing 4 words
        sync(4 * sizeof(uint32_t));    // NOLINT
      }
      break;
    case symbol_type::control_packet_48:
      // value is a bo address
      patch_ctrl48(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first) {
        // Data in this case is written till 3rd offset of bd_data_ptr
        // so syncing 4 words
        sync(4 * sizeof(uint32_t));    // NOLINT
      }
      break;
    case symbol_type::shim_dma_48:
      // value is a bo address
      patch_shim48(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first) {
        // syncing 3 words
        sync(3 * sizeof(uint32_t));    // NOLINT
      }
      break;
    case symbol_type::control_packet_57_aie4:
      // value is a bo address
      patch_ctrl57_aie4(bd_data_ptr, value + config.offset_to_base_bo_addr);
      if (!first) {
        // syncing 3 words
        sync(3 * sizeof(uint32_t));    // NOLINT
      }
      break;
    default:
      throw std::runtime_error("Unsupported symbol type");
    }
  }
}

void
symbol_patcher::
patch_symbol_raw(uint8_t* base, uint64_t value, const patcher_config& config)
{
  for (const auto& cfg : config.m_patch_configs) {
    auto offset = cfg.offset_to_patch_buffer;
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
    default:
      throw std::runtime_error("Unsupported symbol type");
    }
  }
}

} // namespace xrt_core::elf_patcher
