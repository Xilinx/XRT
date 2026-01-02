// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_ELF_PATCHER_H
#define XRT_CORE_ELF_PATCHER_H

#include "module_int.h"
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

// struct symbol_patcher - patcher for a symbol
//
// Manage patching of a symbol in the control code, ctrlpkt etc.
// The symbol type is used to determine the patching method.
//
// The patcher is created with an offset into a buffer object.
// The base address of the buffer object is passed
// in as a parameter to patch.
struct symbol_patcher
{
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

  buf_type m_buf_type = buf_type::ctrltext;
  symbol_type m_symbol_type = symbol_type::shim_dma_48;

  // Maximum BD data words - AIE2P uses 8, AIE4/AIE2PS uses 9
  static constexpr size_t max_bd_words = 9;

  struct patch_info {
    uint64_t offset_to_patch_buffer;
    uint32_t offset_to_base_bo_addr;
    uint32_t mask; // This field is valid only when patching scheme is scalar_32bit_kind
    bool dirty = false; // Tells whether this entry is already patched or not
    uint32_t bd_data_ptrs[max_bd_words]; // array to store bd ptrs original values
  };

  // Each symbol can be patched at multiple places in the buffer.
  // This vector stores the patch information for each place.
  std::vector<patch_info> m_patch_infos;

  // constructor that takes the symbol type, patch information and buffer type.
  symbol_patcher(symbol_type type, std::vector<patch_info> patch_infos, buf_type t);

  // Functions used for patching a symbol in the buffer.
  void patch_symbol(uint8_t* base, uint64_t value);
  void patch_symbol(xrt::bo bo, uint64_t value, bool first);

private:
  // Different patching functions for different symbol types.
  void patch64(uint32_t* data_to_patch, uint64_t addr) const;
  void patch32(uint32_t* data_to_patch, uint64_t register_value, uint32_t mask) const;
  void patch57(uint32_t* bd_data_ptr, uint64_t patch) const;
  void patch57_aie4(uint32_t* bd_data_ptr, uint64_t patch) const;
  void patch_ctrl57(uint32_t* bd_data_ptr, uint64_t patch) const;
  void patch_ctrl48(uint32_t* bd_data_ptr, uint64_t patch) const;
  void patch_shim48(uint32_t* bd_data_ptr, uint64_t patch) const;
  void patch_ctrl57_aie4(uint32_t* bd_data_ptr, uint64_t patch) const;

  template<typename T>
  void
  patch_symbol_helper(T base_or_bo, uint64_t new_value, bool first)
  {
    // base_or_bo is either a pointer to base address of buffer to be patched
    // or xrt::bo object itself
    // shim tests call this function with address directly and call sync themselves
    // but when xrt::bo is passed, we need to call sync explicitly only if its not the
    // first time patching, as in first time patching entire bo is synced
    uint8_t* base = nullptr;

    if constexpr (std::is_same_v<T, xrt::bo>) {
      base = reinterpret_cast<uint8_t*>(base_or_bo.map());
    }
    else
      base = base_or_bo;

    for (auto& item : m_patch_infos) {
      auto offset = item.offset_to_patch_buffer;
      auto bd_data_ptr = reinterpret_cast<uint32_t*>(base + offset);

      if (!item.dirty) {
        // first time patching cache bd ptr values using bd ptrs array in patch info
        std::copy(bd_data_ptr, bd_data_ptr + max_bd_words, item.bd_data_ptrs);
        item.dirty = true;
      }
      else {
        // not the first time patching, restore bd ptr values from patch info bd ptrs array
        std::copy(item.bd_data_ptrs, item.bd_data_ptrs + max_bd_words, bd_data_ptr);
      }

      // lambda for calling sync bo if template arg is of type xrt::bo
      // We only sync the words that are patched not the entire bo
      auto sync = [&](size_t size) {
        if constexpr (std::is_same_v<T, xrt::bo>) {
          base_or_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, size, offset);
        }
      };

      switch (m_symbol_type) {
      case symbol_type::address_64:
        // new_value is a 64bit address
        patch64(bd_data_ptr, new_value);
        if (!first) {
          // sync 64 bits patched
          sync(sizeof(uint64_t));
        }
        break;
      case symbol_type::scalar_32bit_kind:
        // new_value is a register value
        if (item.mask) {
          patch32(bd_data_ptr, new_value, item.mask);
          if (!first) {
            // sync 32 bits patched
            sync(sizeof(uint32_t));
          }
        }
        break;
      case symbol_type::shim_dma_base_addr_symbol_kind:
        // new_value is a bo address
        patch57(bd_data_ptr, new_value + item.offset_to_base_bo_addr);
        if (!first) {
          // Data in this case is written to 8th offset of bd_data_ptr
          // so sync all the words (max_bd_words)
          sync(sizeof(uint32_t) * max_bd_words);
        }
        break;
      case symbol_type::shim_dma_aie4_base_addr_symbol_kind:
        // new_value is a bo address
        patch57_aie4(bd_data_ptr, new_value + item.offset_to_base_bo_addr);
        if (!first) {
          // sync 64 bits or 2 words
          sync(sizeof(uint64_t));
        }
        break;
      case symbol_type::control_packet_57:
        // new_value is a bo address
        patch_ctrl57(bd_data_ptr, new_value + item.offset_to_base_bo_addr);
        if (!first) {
          // Data in this case is written till 3rd offset of bd_data_ptr
          // so syncing 4 words
          sync(4 * sizeof(uint32_t));    // NOLINT
        }
        break;
      case symbol_type::control_packet_48:
        // new_value is a bo address
        patch_ctrl48(bd_data_ptr, new_value + item.offset_to_base_bo_addr);
        if (!first) {
          // Data in this case is written till 3rd offset of bd_data_ptr
          // so syncing 4 words
          sync(4 * sizeof(uint32_t));    // NOLINT
        }
        break;
      case symbol_type::shim_dma_48:
        // new_value is a bo address
        patch_shim48(bd_data_ptr, new_value + item.offset_to_base_bo_addr);
        if (!first) {
          // Data in this case is written till 2nd offset of bd_data_ptr
          // so syncing 3 words
          sync(3 * sizeof(uint32_t));    // NOLINT
        }
        break;
      case symbol_type::control_packet_57_aie4:
        // new_value is a bo address
        patch_ctrl57_aie4(bd_data_ptr, new_value + item.offset_to_base_bo_addr);
        if (!first) {
          // Data in this case is written till 2nd offset of bd_data_ptr
          // so syncing 3 words
          sync(3 * sizeof(uint32_t));    // NOLINT
        }
        break;
      default:
        throw std::runtime_error("Unsupported symbol type");
      }
    }
  }
};

} // namespace xrt_core::elf_patcher

#endif // XRT_CORE_ELF_PATCHER_H
