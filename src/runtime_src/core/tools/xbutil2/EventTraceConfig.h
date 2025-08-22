// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "core/common/json/nlohmann/json.hpp"

// Forward declaration for device
namespace xrt_core { class device; }

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
    bool forced_id;            // Whether ID was explicitly set
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
    int pair_id;                          // Paired event ID (for start/done pairs)
    bool forced_id;                       // Whether ID was explicitly set
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
  event_trace_config(const std::string& json_file_path = "trace_events.json");

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
   * @brief Check if configuration is valid (loaded successfully)
   * @return true if valid, false otherwise
   */
  bool
  is_valid() const { return config_valid; }

  /**
   * @brief Get last error message
   * @return Error message string
   */
  const std::string&
  get_error() const { return last_error; }

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
   * @brief Load configuration from JSON file using nlohmann::json
   * @param json_file_path Path to JSON file
   * @return true on success, false on failure
   */
  bool
  load_from_json(const std::string& json_file_path);

  /**
   * @brief Load and validate JSON file
   * @param json_file_path Path to JSON file
   * @return nlohmann::json object for the loaded file
   * @throws std::runtime_error if file cannot be loaded
   */
  nlohmann::json
  load_json_file(const std::string& json_file_path);

  /**
   * @brief Parse data_format section from JSON
   * @param config Root JSON object
   * @throws std::runtime_error if parsing fails
   */
  void
  parse_data_format(const nlohmann::json& config);

  /**
   * @brief Parse version information from JSON
   * @param config Root JSON object
   * @throws std::runtime_error if version parsing fails
   */
  void
  parse_version(const nlohmann::json& config);

  /**
   * @brief Parse lookups section from JSON (optional)
   * @param config Root JSON object
   */
  void
  parse_lookups(const nlohmann::json& config);

  /**
   * @brief Parse categories section from JSON
   * @param config Root JSON object
   * @throws std::runtime_error if parsing fails
   */
  void
  parse_categories(const nlohmann::json& config);

  /**
   * @brief Create category info from JSON object
   * @param category JSON object for single category
   * @param forced_id_categories Set of already used forced IDs
   * @return category_info structure
   * @throws std::runtime_error if validation fails
   */
  category_info
  create_category_info(const nlohmann::json& category, 
                      std::set<uint32_t>& forced_id_categories);

  /**
   * @brief Assign IDs to categories without forced IDs
   * @param forced_id_categories Set of forced category IDs to avoid
   */
  void
  assign_category_ids(const std::set<uint32_t>& forced_id_categories);

  /**
   * @brief Parse arg_sets section from JSON (optional)
   * @param config Root JSON object
   * @throws std::runtime_error if parsing fails
   */
  void
  parse_arg_sets(const nlohmann::json& config);

  /**
   * @brief Parse a list of arguments for an arg_set
   * @param arg_list JSON array containing argument objects
   * @param arg_set_name Name of the arg_set for error reporting
   * @return Vector of parsed event_arg structures
   * @throws std::runtime_error if validation fails
   */
  std::vector<event_arg>
  parse_argument_list(const nlohmann::json& arg_list, 
                     const std::string& arg_set_name);

  /**
   * @brief Create event_arg from JSON object
   * @param arg_data JSON object for single argument
   * @param start_position Starting bit position for this argument
   * @param arg_set_name Name of containing arg_set for error reporting
   * @return event_arg structure
   * @throws std::runtime_error if validation fails
   */
  event_arg
  create_event_arg(const nlohmann::json& arg_data, uint32_t start_position,
                  const std::string& arg_set_name);

  /**
   * @brief Parse events section from JSON
   * @param config Root JSON object
   * @throws std::runtime_error if parsing fails
   */
  void
  parse_events(const nlohmann::json& config);

  /**
   * @brief Create event_info from JSON object
   * @param event_data JSON object for single event
   * @param name_check Set of used event names for duplicate detection
   * @return event_info structure
   * @throws std::runtime_error if validation fails
   */
  event_info
  create_event_info(const nlohmann::json& event_data, 
                   std::set<std::string>& name_check);

  /**
   * @brief Parse and validate event categories
   * @param event_data JSON object for event
   * @param event Event info to populate
   * @throws std::runtime_error if category references are invalid
   */
  void
  parse_event_categories(const nlohmann::json& event_data, event_info& event);

  /**
   * @brief Parse event arguments reference
   * @param event_data JSON object for event
   * @param event Event info to populate
   * @throws std::runtime_error if arg_set reference is invalid
   */
  void
  parse_event_arguments(const nlohmann::json& event_data, event_info& event);

  /**
   * @brief Assign IDs to events without forced IDs
   * @param events_with_forced_id Map of events with forced IDs
   * @param events_without_id Vector of events needing ID assignment
   */
  void
  assign_event_ids(std::map<uint16_t, event_info>& events_with_forced_id,
                  std::vector<event_info>& events_without_id);

  /**
   * @brief Process START/DONE event pairs and link them
   * @param events_map Map of all events to process
   */
  void
  process_event_pairs(std::map<uint16_t, event_info>& events_map);

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
  uint32_t event_bits;                                     // Event ID bit width
  uint32_t payload_bits;                                   // Payload bit width
  uint16_t file_major;                                     // JSON file major version
  uint16_t file_minor;                                     // JSON file minor version
  std::map<std::string, std::map<uint32_t, std::string>> code_tables;  // Numeric code to string lookup tables
  std::map<std::string, category_info> category_map;      // Category name -> info
  std::map<std::string, std::vector<event_arg>> arg_templates;  // Argument set definitions
  std::map<uint16_t, event_info> event_map;               // Event ID -> info
  
  // Status
  bool config_valid;             // Whether configuration is valid
  std::string last_error;        // Last error message
};
