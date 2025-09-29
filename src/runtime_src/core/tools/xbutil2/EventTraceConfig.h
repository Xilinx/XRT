// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef EVENT_TRACE_CONFIG_H
#define EVENT_TRACE_CONFIG_H

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "core/common/json/nlohmann/json.hpp"

// Forward declaration for device
namespace xrt_core { class device; }

namespace xrt_core::tools::xrt_smi{
/**
 * @brief Raw event record structure
 */
struct event_record
{
  uint64_t timestamp;
  uint16_t event_id;
  uint64_t payload;
};

/**
 * @brief Configuration loader for firmware event trace data
 * 
 * This class reads event trace configuration from trace_events.json
 * using nlohmann::json and provides methods to parse and interpret 
 * firmware trace events.
 */
class event_trace_config {
public:
  /**
   * @brief Individual argument definition for an event
   */
  struct event_arg {
    std::string name;           // Argument name (e.g., "context_id")
    uint32_t width;             // Bit width of the argument
    uint32_t start;             // Starting bit position in payload (calculated)
    std::string format;         // Display format (e.g., "08x", "d")
    std::string lookup;         // Lookup table name (if any)
    bool signed_field;          // Whether the field is signed
    std::string description;    // Human-readable description
  };

  /**
   * @brief Category definition from YAML configuration
   */
  struct category_info {
    std::string name;           // Category name
    std::string description;    // Category description
    uint32_t id;               // Category bit ID
  };

  /**
   * @brief Event definition from YAML configuration
   */
  struct event_info {
    uint16_t id;                           // Event ID (index in events array)
    std::string name;                      // Event name (e.g., "FRAME_START")
    std::string description;               // Event description
    std::vector<std::string> categories;   // Event category names
    uint32_t category_mask;                // Category bitmask
    std::string args_name;                 // Argument set name (from arg_sets)
    std::vector<event_arg> args;           // Resolved event arguments
    std::string type;                      // Event type (start/done/null)
  };

  /**
   * @brief Parsed event data from firmware buffer
   */
  struct parsed_event {
    uint64_t timestamp;                    // Event timestamp
    uint16_t event_id;                     // Event ID
    std::string name;                      // Event name
    std::string description;               // Event description
    std::vector<std::string> categories;   // Event categories
    std::map<std::string, std::string> args; // Parsed arguments (name -> formatted value)
    uint64_t raw_payload;                  // Raw payload data
  };

public:
  /**
   * @brief Constructor - loads configuration from JSON file
   * @param json_file_path Path to trace_events.json file
   * 
   * After construction, call validate_version_compatibility() with a device
   * to check if the JSON file version matches the device/shim version.
   */
  explicit
  event_trace_config(const nlohmann::json& json_config);

  /**
   * @brief Parse a single trace event from raw data
   * @param record Raw event record
   * @return Parsed event structure
   */
  parsed_event
  parse_event(const event_record& record) const;

  /**
   * @brief Get event name by ID
   * @param event_id Event ID to look up
   * @return Event name or "UNKNOWN" if not found
   */
  std::string
  get_event_name(uint16_t event_id) const;

  /**
   * @brief Get event categories by ID
   * @param event_id Event ID to look up
   * @return Vector of category names
   */
  std::vector<std::string>
  get_event_categories(uint16_t event_id) const;

  /**
   * @brief Get data format information
   * @return pair of (event_bits, payload_bits)
   */
  std::pair<uint32_t, uint32_t>
  get_data_format() const { 
    return {event_bits, payload_bits}; 
  }

  /**
   * @brief Get JSON file version
   * @return pair of (major, minor) version from JSON file
   */
  std::pair<uint16_t, uint16_t>
  get_file_version() const {
    return {file_major, file_minor};
  }


private:
  /**
   * @brief Parse event_bits from JSON data_format
   * @param config Root JSON object
   * @return event_bits (uint32_t)
   */
  static uint32_t parse_event_bits(const nlohmann::json& config);

  /**
   * @brief Parse payload_bits from JSON data_format
   * @param config Root JSON object
   * @return payload_bits (uint32_t)
   */
  static uint32_t parse_payload_bits(const nlohmann::json& config);

  /**
   * @brief Parse major version from JSON
   * @param config Root JSON object
   * @return major version (uint16_t)
   */
  static uint16_t 
  parse_major_version(const nlohmann::json& config);

  /**
   * @brief Parse minor version from JSON
   * @param config Root JSON object
   * @return minor version (uint16_t)
   */
  static uint16_t 
  parse_minor_version(const nlohmann::json& config);

  /**
   * @brief Parse lookups section from JSON (optional)
   * @param config Root JSON object
   * @return code_tables map
   */
  static std::map<std::string, std::map<uint32_t, std::string>>
  parse_code_table(const nlohmann::json& config);

  /**
   * @brief Parse categories section from JSON
   * @param config Root JSON object
   * @throws std::runtime_error if parsing fails
   */
  static std::map<std::string, category_info>
  parse_categories(const nlohmann::json& config);

  /**
   * @brief Create category info from JSON object
   * @param category JSON object for single category
   * @param forced_id_categories Set of already used forced IDs
   * @return category_info structure
   * @throws std::runtime_error if validation fails
   */
  static category_info
  create_category_info(const nlohmann::json& category);

  /**
   * @brief Parse arg_sets section from JSON (optional)
   * @param config Root JSON object
   * @throws std::runtime_error if parsing fails
   */
  static std::map<std::string, std::vector<event_arg>>
  parse_arg_sets(const nlohmann::json& config, uint32_t payload_bits);

  /**
   * @brief Parse a list of arguments for an arg_set
   * @param arg_list JSON array containing argument objects
   * @param arg_set_name Name of the arg_set for error reporting
   * @return Vector of parsed event_arg structures
   * @throws std::runtime_error if validation fails
   */
  static std::vector<event_arg>
  parse_argument_list(const nlohmann::json& arg_list, 
                      const std::string& arg_set_name, uint32_t payload_bits);

  /**
   * @brief Create event_arg from JSON object
   * @param arg_data JSON object for single argument
   * @param start_position Starting bit position for this argument
   * @param arg_set_name Name of containing arg_set for error reporting
   * @return event_arg structure
   * @throws std::runtime_error if validation fails
   */
  static event_arg
  create_event_arg(const nlohmann::json& arg_data, uint32_t start_position,
                  const std::string& arg_set_name);

  /**
   * @brief Parse events section from JSON
   * @param config Root JSON object
   * @throws std::runtime_error if parsing fails
   */
  static std::map<uint16_t, event_info>
  parse_events(const nlohmann::json& config, 
               const std::map<std::string, category_info>& category_map, 
               const std::map<std::string, std::vector<event_arg>>& arg_templates);

  /**
   * @brief Create event_info from JSON object
   * @param event_data JSON object for single event
   * @param name_check Set of used event names for duplicate detection
   * @return event_info structure
   * @throws std::runtime_error if validation fails
   */
  static event_info
  create_event_info(const nlohmann::json& event_data, const std::map<std::string, category_info>& category_map, const std::map<std::string, std::vector<event_arg>>& arg_templates);

  /**
   * @brief Parse and validate event categories
   * @param event_data JSON object for event
   * @param event Event info to populate
   * @throws std::runtime_error if category references are invalid
   */
  static void
  parse_event_categories(const nlohmann::json& event_data, event_info& event, const std::map<std::string, category_info>& category_map);

  /**
   * @brief Parse event arguments reference
   * @param event_data JSON object for event
   * @param event Event info to populate
   * @throws std::runtime_error if arg_set reference is invalid
   */
  static void
  parse_event_arguments(const nlohmann::json& event_data, event_info& event, const std::map<std::string, std::vector<event_arg>>& arg_templates);

  /**
   * @brief Extract argument value from payload
   * @param payload Raw payload data
   * @param arg Argument definition
   * @return Formatted argument value
   * @throws std::runtime_error if extraction fails
   */
  std::string
  extract_arg_value(uint64_t payload, const event_arg& arg) const;

  /**
   * @brief Format value according to format specification
   * @param value Raw value
   * @param format Format string
   * @return Formatted string
   */
  std::string
  format_value(uint64_t value, const std::string& format) const;

private:
  // Configuration data
  nlohmann::json config;
  uint32_t event_bits;                                     // Event ID bit width
  uint32_t payload_bits;                                   // Payload bit width
  uint16_t file_major;                                     // JSON file major version
  uint16_t file_minor;                                     // JSON file minor version
  std::map<std::string, std::map<uint32_t, std::string>> code_tables;  // Numeric code to string lookup tables
  std::map<std::string, category_info> category_map;      // Category name -> info
  std::map<std::string, std::vector<event_arg>> arg_templates;  // Argument set definitions
  std::map<uint16_t, event_info> event_map;               // Event ID -> info
};

} // namespace xrt_core::tools::xrt_smi

#endif // EVENT_TRACE_CONFIG_H
