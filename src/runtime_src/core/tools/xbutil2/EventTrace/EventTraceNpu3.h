// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef CONFIG_NPU3_H
#define CONFIG_NPU3_H

#include "EventTraceBase.h"
#include "core/common/json/nlohmann/json.hpp"

#include <map>
#include <string>
#include <vector>
#include <optional>

// Forward declaration for device
namespace xrt_core { class device; }

namespace xrt_core::tools::xrt_smi{

constexpr uint8_t npu3_magic_byte = 0xAA;
constexpr size_t npu3_timestamp_bytes = 8;
constexpr size_t npu3_header_bytes = 12; // timestamp(8) + magic(1) + category_id(2) + payload_size(1)

/**
 * @brief NPU3-specific event trace configuration
 * 
 * Handles NPU3 format with variable-size events and struct-based payload.
 * Format: [timestamp:8][magic:0xAA][category_id:2][payload_size:1][payload:variable]
 */
class config_npu3 : public event_trace_config {
public:
  /**
   * @brief NPU3 argument definition (extends base with type/count)
   */
  struct event_arg_npu3 : public event_arg {
    std::string type;   // C type (uint8, uint16, uint32, uint64)
    uint32_t count;     // Array count
  };

  /**
   * @brief NPU3 event definition (extends base with args vector)
   */
  struct event_info_npu3 : public event_info {
    std::vector<event_arg_npu3> args;
  };

  /**
   * @brief NPU3 event data with payload pointer
   */
  struct event_data_t {
    uint64_t timestamp;
    uint16_t category_id;
    const uint8_t* payload_ptr;
    uint8_t payload_size;
  };

public:
  /**
   * @brief Constructor
   * @param json_config Configuration file
   */
  explicit config_npu3(nlohmann::json json_config);

  /**
   * @brief Load configuration from a device
   * @param device Device to load from
   * @return Optional containing config if successful
   */
  static std::optional<config_npu3>
  load_config(const xrt_core::device* device);

  /**
   * @brief Parse NPU3 event from buffer
   * @param buffer_ptr Pointer to event data
   * @return Event data structure
   */
  event_data_t
  parse_buffer(const uint8_t* buffer_ptr) const;

  /**
   * @brief Decode event into human-readable form
   * @param event_data Event data
   * @return Decoded event
   */
  decoded_event_t
  decode_event(const event_data_t& event_data) const;

  /**
   * @brief Get event name by ID
   * @param event_id Event ID
   * @return Event name or "UNKNOWN"
   */
  std::string
  get_event_name(uint16_t event_id) const;

  /**
   * @brief Get event info by ID
   * @param event_id Event ID
   * @return Optional event info
   */
  std::optional<event_info_npu3>
  get_event_info(uint16_t event_id) const;

  /**
   * @brief Get file version
   * @return pair of (major, minor)
   */
  std::pair<uint16_t, uint16_t>
  get_file_version() const {
    return {m_file_major, m_file_minor};
  }

private:
  // Parse argument templates from JSON
  std::map<std::string, std::vector<event_arg_npu3>>
  parse_arg_sets();

  std::vector<event_arg_npu3>
  parse_argument_list(const nlohmann::json& arg_list,
                      const std::string& arg_set_name);

  event_arg_npu3
  create_event_arg(const nlohmann::json& arg_data,
                   const std::string& arg_set_name);

  // Parse events from JSON
  std::map<uint16_t, event_info_npu3> 
  parse_events();

  event_info_npu3 
  create_event_info(const nlohmann::json& event_data);

  void 
  parse_event_categories(const nlohmann::json& event_data, event_info_npu3& event);

  void 
  parse_event_arguments(const nlohmann::json& event_data, event_info_npu3& event);

  // Extract and format argument values from payload
  std::string 
  extract_arg_value(const uint8_t* payload_ptr,
                    size_t& offset,
                    const event_arg_npu3& arg) const;

  size_t 
  get_type_size(const std::string& type) const;

  std::string 
  format_value(uint64_t value, 
               const std::string& format) const;

private:
  // NPU3-specific configuration data
  std::map<std::string, std::vector<event_arg_npu3>> m_arg_templates;
  std::map<uint16_t, event_info_npu3> m_event_map;
};

/**
 * @brief NPU3 event trace parser
 */
class parser_npu3 : public event_trace_parser {
public:
  using event_data_t = config_npu3::event_data_t;
  using decoded_event_t = event_trace_config::decoded_event_t;

  /**
   * @brief Constructor
   * @param config Event trace configuration
   */
  explicit 
  parser_npu3(const config_npu3& config);

  /**
   * @brief Parse raw event trace buffer
   * @param data_ptr Pointer to data
   * @param buf_size Buffer size
   * @return Formatted string
   */
  std::string 
  parse(const uint8_t* data_ptr, size_t buf_size) const;

private:
  // Format decoded event as table row
  std::string format_event(const decoded_event_t& decoded_event) const;

  // Reference to configuration
  const config_npu3& m_config;
};

} // namespace xrt_core::tools::xrt_smi

#endif // CONFIG_NPU3_H
