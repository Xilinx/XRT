// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef EVENT_TRACE_BASE_H
#define EVENT_TRACE_BASE_H

#include "core/common/device.h"
#include "core/common/json/nlohmann/json.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace xrt_core::tools::xrt_smi{

/**
 * @brief Base class for event trace configuration
 * 
 * Provides common functionality shared between STRx and NPU3 implementations.
 */
class event_trace_config {

public:
  // Default copy and move operations
  event_trace_config(const event_trace_config&) = default;

  event_trace_config& 
  operator=(const event_trace_config&) = default;

  event_trace_config(event_trace_config&&) = default;

  event_trace_config& 
  operator=(event_trace_config&&) = default;

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
   * @brief Get category map from device (static utility)
   * @param device Device to query
   * @return Map of category name to mask
   */
  static std::map<std::string, uint32_t>
  get_category_map(const xrt_core::device* device);

  /**
   * @brief Convert category mask to category names (static utility)
   * @param mask Category mask to decode
   * @param device Device to query for category definitions
   * @return Vector of category names
   */
  static std::vector<std::string>
  mask_to_category_names(uint32_t mask, const xrt_core::device* device);

  /**
   * @brief Get the entry header size
   * @return Entry header size in bytes
   */
  size_t 
  get_entry_header_size() const 
  { 
    return m_entry_header_size; 
  }

  /**
   * @brief Get the entry footer size
   * @return Entry footer size in bytes
   */
  size_t 
  get_entry_footer_size() const 
  { 
    return m_entry_footer_size; 
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
  explicit 
  event_trace_config(nlohmann::json json_config);

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

  /**
   * @brief Parse structure size from JSON structures section
   * @param struct_name Name of structure to get size for
   * @return Size in bytes, or 0 if not found
   */
  size_t
  parse_structure_size(const std::string& struct_name);

  // Protected accessors for derived classes
  const nlohmann::json& 
  get_config() const 
  { return m_config; }

  uint16_t 
  get_file_major() const 
  { return m_file_major; }

  uint16_t 
  get_file_minor() const 
  { return m_file_minor; }

  const std::map<std::string, std::map<uint32_t, std::string>>& 
  get_code_tables() const 
  { return m_code_tables; }

  const std::map<std::string, category_info>& 
  get_category_map() const 
  { return m_category_map; }

  /**
   * @brief Get file version
   * @return pair of (major, minor)
   */
  std::pair<uint16_t, uint16_t>
  get_file_version() const {
    return {m_file_major, m_file_minor};
  }

private:
  // Members common between strix and npu3
  nlohmann::json m_config;
  uint16_t m_file_major;
  uint16_t m_file_minor;
  std::map<std::string, std::map<uint32_t, std::string>> m_code_tables;
  std::map<std::string, category_info> m_category_map;
  size_t m_entry_header_size;                                // Entry header size in bytes
  size_t m_entry_footer_size;                                // Entry footer size in bytes
};

/**
 * @brief Base class for event trace parser
 * 
 * Provides common formatting functionality shared between STRx and NPU3 parsers.
 */
class event_trace_parser {

  /** Column widths for table: [timestamp, event_name, category, arguments] */
  std::vector<size_t> m_column_widths;

public:
  /**
   * @brief Factory method to create appropriate parser based on device type
   * @param config Config to create parser for
   * @param device Device to determine parser type from
   * @return Unique pointer to parser (STRx or NPU3)
   */
  static std::unique_ptr<event_trace_parser>
  create_from_config(const std::unique_ptr<event_trace_config>& config, 
                     const xrt_core::device* device);

  /**
   * @brief Parse raw event trace buffer to formatted string (pure virtual)
   * @param data_ptr Pointer to raw event trace data buffer
   * @param buf_size Size of the data buffer in bytes
   * @return Formatted event trace output with parsed events
   */
  virtual std::string
  parse(const uint8_t* data_ptr, 
        size_t buf_size) const = 0;

  /**
   * @brief Get formatted header row for the event trace table
   * @return std::string Formatted header row (Timestamp, Event Name, Category, Arguments)
   */
  std::string
  get_header_row() const;

  /**
   * @brief Virtual destructor (public for unique_ptr)
   */
  virtual ~event_trace_parser() = default;

  // Default copy and move operations
  event_trace_parser(const event_trace_parser&) = default;

  event_trace_parser& 
  operator=(const event_trace_parser&) = default;

  event_trace_parser(event_trace_parser&&) = default;

  event_trace_parser& 
  operator=(event_trace_parser&&) = default;

protected:
  /**
   * @brief Protected default constructor (uses default column widths)
   */
  event_trace_parser();

  /**
   * @brief Format a single event row using stored column widths
   * @param timestamp Event timestamp
   * @param event_name Event name (or "UNKNOWN")
   * @param category_display Formatted categories string
   * @param args_str Formatted arguments string
   * @return Formatted table row string
   */
  std::string
  format_event_row(uint64_t timestamp,
                   const std::string& event_name,
                   const std::string& category_display,
                   const std::string& args_str) const;

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
