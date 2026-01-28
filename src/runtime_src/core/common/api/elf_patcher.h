// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_ELF_PATCHER_H
#define XRT_CORE_ELF_PATCHER_H

#include "core/common/config.h"
#include "xrt/xrt_bo.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

// This file contains the patching logic related to xrt::elf
namespace xrt_core::elf_patcher {

// enum with different buffer types that supports patching
enum class buf_type {
  ctrltext = 0,        // control code
  ctrldata = 1,        // control packet
  preempt_save = 2,    // preempt_save
  preempt_restore = 3, // preempt_restore
  pdi = 4,             // pdi
  ctrlpkt_pm = 5,      // preemption ctrl pkt
  pad = 6,             // scratchpad/control packet section name for next gen aie devices
  dump = 7,            // dump section containing debug info for trace etc
  ctrlpkt = 8,         // control packet section in aie2ps/aie4 new ELFs
  buf_type_count = 9   // total number of buf types
};

// Section name patterns corresponding to buf_type enum values
inline constexpr std::array<std::string_view, static_cast<int>(buf_type::buf_type_count)>
section_names = {
  ".ctrltext",       // ctrltext
  ".ctrldata",       // ctrldata
  ".preempt_save",   // preempt_save
  ".preempt_restore",// preempt_restore
  ".pdi",            // pdi
  ".ctrlpkt.pm",     // ctrlpkt_pm
  ".pad",            // pad
  ".dump",           // dump
  ".ctrlpkt"         // ctrlpkt
};

// Get section name pattern for a buffer type
inline constexpr std::string_view
get_section_name(buf_type type)
{
  return section_names[static_cast<int>(type)];
}

// Generate key string for patcher lookup
XRT_CORE_UNUSED
inline std::string
generate_key_string(const std::string& argument_name, buf_type type)
{
  return argument_name + std::to_string(static_cast<int>(type));
}

// Symbol type enum for patching schemes
enum class symbol_type {
  uc_dma_remote_ptr_symbol_kind = 1,
  shim_dma_base_addr_symbol_kind = 2,      // patching scheme needed by AIE2PS firmware
  scalar_32bit_kind = 3,
  control_packet_48 = 4,                   // patching scheme needed by firmware to patch control packet
  shim_dma_48 = 5,                         // patching scheme needed by firmware to patch instruction buffer
  shim_dma_aie4_base_addr_symbol_kind = 6, // patching scheme needed by AIE4 firmware
  control_packet_57 = 7,                   // patching scheme needed by firmware to patch control packet for aie2ps
  address_64 = 8,                          // patching scheme needed to patch pdi address
  control_packet_57_aie4 = 9,              // patching scheme needed by firmware to patch control packet for aie4
  unknown_symbol_kind = 10
};

// Maximum BD data words - AIE2P uses 8, AIE4/AIE2PS uses 9
static constexpr size_t max_bd_words = 9;

// Patching system for control code
//
// The patching system uses a split design for thread-safety:
// - patch_config: Static configuration (shared, read-only after ELF parsing)
// - patch_state: Runtime state (per-instance, mutable)
// - symbol_patcher: Per-instance patcher that holds config pointer + owns state
//
// module_elf parses ELF and creates patch_config objects (shared across instances)
// module_run creates symbol_patcher objects with their own patch_state (thread-safe)

struct patch_config {
  uint64_t offset_to_patch_buffer;
  uint32_t offset_to_base_bo_addr;
  uint32_t mask; // This field is valid only when patching scheme is scalar_32bit_kind
};

// Runtime state per patch location - owned by module_run
struct patch_state {
  bool dirty = false; // Tells whether this entry is already patched or not
  std::array<uint32_t, max_bd_words> bd_data_ptrs = {}; // array to store bd ptrs original values
};

// struct patcher_config - static configuration for a patcher
//
// Stored in elf_impl, shared across module_run instances (read-only)
// Contains symbol type, buffer type, and all patch locations
struct patcher_config
{
  symbol_type m_symbol_type = symbol_type::shim_dma_48;
  buf_type m_buf_type = buf_type::ctrltext;
  std::vector<patch_config> m_patch_configs;

  // Constructor for creating config during ELF parsing
  patcher_config(symbol_type type, std::vector<patch_config> configs, buf_type t);

  void
  add_patch(const patch_config& pc);
};

// struct symbol_patcher - runtime patcher for a symbol
//
// Created by module_run, references shared config from elf_impl,
// owns its own runtime state for thread-safe patching.
struct symbol_patcher
{
  // Pointer to shared static configuration (owned by elf_impl)
  const patcher_config* m_config = nullptr;

  // Runtime state per patch location
  std::vector<patch_state> m_states;

  // Constructor - takes pointer to shared config, initializes state
  explicit symbol_patcher(const patcher_config* config);

  // Function to patch a symbol in the buffer.
  void
  patch_symbol(xrt::bo bo, uint64_t value, bool first);

  // static method for patching raw buffers passed by shim tests
  // where the caller handles sync themselves
  // It patches directly using config without maintaining state.
  static void
  patch_symbol_raw(uint8_t* base, uint64_t value, const patcher_config& config);

private:
  // Different patching functions for different symbol types.
  static void
  patch64(uint32_t* data_to_patch, uint64_t addr);

  static void
  patch32(uint32_t* data_to_patch, uint64_t register_value, uint32_t mask);

  static void
  patch57(uint32_t* bd_data_ptr, uint64_t patch);

  static void
  patch57_aie4(uint32_t* bd_data_ptr, uint64_t patch);

  static void
  patch_ctrl57(uint32_t* bd_data_ptr, uint64_t patch);

  static void
  patch_ctrl48(uint32_t* bd_data_ptr, uint64_t patch);

  static void
  patch_shim48(uint32_t* bd_data_ptr, uint64_t patch);

  static void
  patch_ctrl57_aie4(uint32_t* bd_data_ptr, uint64_t patch);
};

} // namespace xrt_core::elf_patcher

#endif // XRT_CORE_ELF_PATCHER_H
