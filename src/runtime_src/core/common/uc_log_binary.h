// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrtcore_common_uc_log_binary_h_
#define xrtcore_common_uc_log_binary_h_

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace xrt_core {

/// UC device log ring entry (binary layout). Used by buffer_dumper and host-side tools
/// (e.g. WinDbg extension) without pulling in buffer_dumper / xrt::bo.
struct uc_log_entry
{
  uint32_t length = 0;    // Log entry length in number of words
  uint32_t ts_high = 0;   // Timestamp high 32 bits
  uint32_t ts_low = 0;    // Timestamp low 32 bits
  uint32_t file_id = 0;   // ID of log source file
  uint32_t line_num = 0;  // Line number of log in source file
  uint32_t log_id = 0;    // ID of format string (see uc_log_schema.h)
  uint32_t argument1 = 0; // First argument (present if length > word offset of argument1)
  uint32_t argument2 = 0; // Second argument (present if length > word offset of argument2)
};

#define UC_LOG_ENTRY_SIZE  32
static_assert(std::is_standard_layout_v<uc_log_entry>, "uc_log_entry layout");
static_assert(sizeof(uc_log_entry) == UC_LOG_ENTRY_SIZE, "uc_log_entry size");

/// Chunk metadata header size (one uc_log_entry) preceding log records in a chunk
inline constexpr std::size_t uc_log_chunk_metadata_size = sizeof(uc_log_entry);

inline constexpr uint32_t uc_log_entry_word_offset_argument1 =
    static_cast<uint32_t>(offsetof(uc_log_entry, argument1) / sizeof(uint32_t));
inline constexpr uint32_t uc_log_entry_word_offset_argument2 =
    static_cast<uint32_t>(offsetof(uc_log_entry, argument2) / sizeof(uint32_t));
inline constexpr uint32_t uc_log_entry_word_count_full =
    static_cast<uint32_t>(sizeof(uc_log_entry) / sizeof(uint32_t));

} // namespace xrt_core

#endif
