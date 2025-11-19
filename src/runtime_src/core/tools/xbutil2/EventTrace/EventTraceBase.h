// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef EVENT_TRACE_BASE_H
#define EVENT_TRACE_BASE_H

#include "core/common/json/nlohmann/json.hpp"
#include "core/common/device.h"

#include <map>
#include <string>
#include <vector>
#include <cstdint>

namespace xrt_core::tools::xrt_smi{

/**
 * @brief Base class for event trace configuration
 * 
 * Provides common functionality shared between STRx and NPU3 implementations.
 */
class event_trace_config {
public:
  /**
   * @brief Category definition
   */
  struct category_info {
    std::string name;
    std::string description;
    uint32_t id;
  };

  /**
   * @brief Decoded event data (common structure)
   */
  struct decoded_event_t {
    uint64_t timestamp;
    uint16_t event_id;
    std::string name;
    std::string description;
    std::vector<std::string> categories;
    std::map<std::string, std::string> args;
  };

  /**
   * @brief Base argument definition (common fields)
   */
  struct event_arg {
    std::string name;
    std::string format;
    std::string lookup;
    bool signed_field;
    std::string description;
  };

  /**
   * @brief Base event info structure
   */
  struct event_info {
    uint16_t id;
    std::string name;
    std::string description;
    std::vector<std::string> categories;
    uint32_t category_mask;
    std::string args_name;
    std::string type;
  };

  /**
   * @brief Factory method to create appropriate config based on device
   * @param device Device to load from
   * @return Unique pointer to config (STRx or NPU3)
   */
  static std::unique_ptr<event_trace_config>
  create_from_device(const xrt_core::device* device);

  /**
   * @brief Get category map
   * @return Map of category name to category info
   */
  const std::map<std::string, category_info>&
  get_categories() const {
    return m_category_map;
  }

  /**
   * @brief Virtual destructor (public for unique_ptr)
   */
  virtual ~event_trace_config() = default;

protected:
  /**
   * @brief Constructor
   * @param json_config Parsed JSON configuration
   */
  explicit event_trace_config(nlohmann::json json_config);

  /**
   * @brief Load JSON configuration from device
   * @param device Device to load from
   * @return Parsed JSON configuration
   */
  static nlohmann::json
  load_json_from_device(const xrt_core::device* device);

  // Common parsing methods
  uint16_t
  parse_major_version();
  uint16_t
  parse_minor_version();
  std::map<std::string, std::map<uint32_t, std::string>>
  parse_code_table();
  std::map<std::string, category_info>
  parse_categories();
  category_info
  create_category_info(const nlohmann::json& category);

protected:
  // Common data members
  nlohmann::json m_config;
  uint16_t m_file_major;
  uint16_t m_file_minor;
  std::map<std::string, std::map<uint32_t, std::string>> m_code_tables;
  std::map<std::string, category_info> m_category_map;
};

/**
 * @brief Base class for event trace parser
 * 
 * Provides common formatting functionality shared between STRx and NPU3 parsers.
 */
class event_trace_parser {
public:
  /**
   * @brief Factory method to create appropriate parser based on device type
   * @param config Config to create parser for
   * @param device Device to determine parser type from
   * @return Unique pointer to parser (STRx or NPU3)
   */
  static std::unique_ptr<event_trace_parser>
  create_from_config(const std::unique_ptr<event_trace_config>& config, const xrt_core::device* device);

  /**
   * @brief Parse raw event trace buffer to formatted string (pure virtual)
   * @param data_ptr Pointer to raw event trace data buffer
   * @param buf_size Size of the data buffer in bytes
   * @return Formatted event trace output with parsed events
   */
  virtual std::string
  parse(const uint8_t* data_ptr, size_t buf_size) const = 0;

  /**
   * @brief Virtual destructor (public for unique_ptr)
   */
  virtual ~event_trace_parser() = default;

protected:
  /**
   * @brief Protected constructor
   */
  event_trace_parser() = default;

  /**
   * @brief Format event categories for table display (inline format)
   * @param categories Vector of category strings
   * @return Formatted category string without brackets
   */
  std::string
  format_categories(const std::vector<std::string>& categories) const;

  /**
   * @brief Format event arguments for table display (inline format)
   * @param args Map of argument key-value pairs
   * @return Formatted arguments string without parentheses
   */
  std::string
  format_arguments(const std::map<std::string, std::string>& args) const;
};

} // namespace xrt_core::tools::xrt_smi

#endif // EVENT_TRACE_BASE_H
