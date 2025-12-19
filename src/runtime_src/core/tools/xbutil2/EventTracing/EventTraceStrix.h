// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef CONFIG_STRIX_H
#define CONFIG_STRIX_H

#include "EventTraceBase.h"
#include "core/common/json/nlohmann/json.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

// Forward declaration for device
namespace xrt_core { class device; }

namespace xrt_core::tools::xrt_smi{

constexpr uint32_t event_bits_default = 16;
constexpr uint32_t payload_bits_default = 48;
constexpr uint32_t timestamp_bytes_default = 8;

/**
 * @brief Configuration loader for firmware event trace data
 * 
 * This class reads event trace configuration from trace_events.json
 * using nlohmann::json and provides methods to parse and interpret 
 * firmware trace events.
 */
class config_strix : public event_trace_config {
public:
  /**
   * @brief STRx argument definition (extends base with width/start)
   */
  struct event_arg_strix : public event_trace_config::event_arg {
    uint32_t width;  // Bit width of the argument
    uint32_t start;  // Starting bit position in payload
  };



  /**
   * @brief STRx event definition (extends base with args vector)
   */
  struct event_info_strix : 
  public event_trace_config::event_info 
  {
    std::vector<event_arg_strix> args;  // Resolved event arguments
  };

  /**
   * @brief Input event data for decoding
   */
  struct event_data_t {
    uint64_t timestamp;  // Event timestamp
    uint16_t event_id;   // Event ID  
    uint64_t payload;    // Event payload
  };

  /**
   * @brief STRx decoded event (extends base with raw_payload)
   */
  struct decoded_event_t : public event_trace_config::decoded_event_t {
    uint64_t raw_payload;  // Raw payload data
  };

public:
  /**
   * @brief Constructor
   * @param json_config Configuration file
   */
  config_strix(nlohmann::json json_config);

  /**
   * @brief Parse a single trace event from raw values
   * @param timestamp Event timestamp 
   * @param event_id Event ID
   * @param payload Event payload
   * @return Parsed event structure
   */
  decoded_event_t
  decode_event(const event_data_t& event_data) const;

  /**
   * @brief Get event name by ID
   * @param event_id Event ID to look up
   * @return Event name or "UNKNOWN" if not found
   */
  std::string
  get_event_name(uint16_t event_id) const;

  /**
   * @brief Get data format information
   * @return pair of (event_bits, payload_bits)
   */
  std::pair<uint32_t, uint32_t>
  get_data_format() const { 
    return {m_event_bits, m_payload_bits}; 
  }

  /**
   * @brief Calculate size of single event based on config
   * @return Size in bytes for one event
   */
  size_t get_event_size() const {
    return timestamp_bytes_default + (m_event_bits + m_payload_bits) / 8; //NOLINT (cppcoreguidelines-avoid-magic-numbers)
  }

  /**
   * @brief Parse single event from buffer at runtime using config sizes
   * @param buffer_ptr Pointer to event data in buffer
   */
  event_data_t 
  parse_buffer(const uint8_t* buffer_ptr) const;

private:
  /**
   * @brief Parse event_bits from JSON data_format
   * @return event_bits (uint32_t)
   */
  uint32_t 
  parse_event_bits();

  /**
   * @brief Parse payload_bits from JSON data_format
   * @return payload_bits (uint32_t)
   */
  uint32_t 
  parse_payload_bits();

  /**
   * @brief Parse arg_sets section from JSON (optional)
   * @throws std::runtime_error if parsing fails
   */
  std::map<std::string, std::vector<event_arg_strix>>
  parse_arg_sets();

  /**
   * @brief Parse a list of arguments for an arg_set
   * @param arg_list JSON array containing argument objects
   * @param arg_set_name Name of the arg_set for error reporting
   * @return Vector of parsed event_arg structures
   * @throws std::runtime_error if validation fails
   */
  std::vector<event_arg_strix>
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
  event_arg_strix
  create_event_arg(const nlohmann::json& arg_data, 
                  uint32_t start_position,
                  const std::string& arg_set_name);

  /**
   * @brief Parse events section from JSON
   * @throws std::runtime_error if parsing fails
   */
  std::map<uint16_t, event_info_strix>
  parse_events();

  /**
   * @brief Create event_info from JSON object
   * @param event_data JSON object for single event
   * @param name_check Set of used event names for duplicate detection
   * @return event_info structure
   * @throws std::runtime_error if validation fails
   */
  event_info_strix
  create_event_info(const nlohmann::json& event_data);

  /**
   * @brief Parse and validate event categories
   * @param event_data JSON object for event
   * @param event Event info to populate
   * @throws std::runtime_error if category references are invalid
   */
  void
  parse_event_categories(const nlohmann::json& event_data,
                         event_info_strix& event);

  /**
   * @brief Parse event arguments reference
   * @param event_data JSON object for event
   * @param event Event info to populate
   * @throws std::runtime_error if arg_set reference is invalid
   */
  void
  parse_event_arguments(const nlohmann::json& event_data, 
                        event_info_strix& event);

  /**
   * @brief Extract argument value from payload
   * @param payload Raw payload data
   * @param arg Argument definition
   * @return Formatted argument value
   * @throws std::runtime_error if extraction fails
   */
  std::string
  extract_arg_value(uint64_t payload, 
                    const event_arg_strix& arg) const;

  /**
   * @brief Format value according to format specification
   * @param value Raw value
   * @param format Format string
   * @return Formatted string
   */
  std::string
  format_value(uint64_t value, 
              const std::string& format) const;

private:
  // STRx-specific configuration data
  uint32_t m_event_bits;                                     // Event ID bit width
  uint32_t m_payload_bits;                                   // Payload bit width
  std::map<std::string, std::vector<event_arg_strix>> m_arg_templates;  // Argument set definitions
  std::map<uint16_t, event_info_strix> m_event_map;               // Event ID -> info
};

/**
 * @brief Event trace parser class for minimal overhead parsing
 *
 * This class takes raw binary event trace data and converts it to
 * human-readable formatted output using configuration-based event definitions.
 *
 * - Constructor takes configuration by reference
 * - Single parse() method processes raw data buffer
 * 
 * Usage:
 * @code
 * event_trace_parser parser(config);
 * std::string result = parser.parse(data_ptr, buf_size);
 * @endcode
 */
class parser_strix : public event_trace_parser {
public:
  using event_data_t = config_strix::event_data_t;
  using decoded_event_t = config_strix::decoded_event_t;

  /**
   * @brief Constructor
   * @param config Event trace configuration
   */
  explicit parser_strix(config_strix config);
  
  /**
   * @brief Parse raw event trace buffer to formatted string
   * 
   * @param data_ptr Pointer to raw event trace data buffer
   * @param buf_size Size of the data buffer in bytes
   * @return std::string Formatted event trace output with parsed events
   * 
   * Processes the raw binary event trace data and converts each event
   * to a human-readable format including:
   * - Event index, timestamp, and ID
   * - Event name from configuration
   */
  std::string 
  parse(const uint8_t* data_ptr, 
        size_t buf_size) const override;

private:

  /**
   * @brief Format summary header
   * @param event_count Number of events found
   * @param buf_size Buffer size in bytes
   * @return Formatted summary string
   */
  std::string 
  format_summary(size_t event_count, 
                 size_t buf_size) const;

  /**
   * @brief Format a single event for output
   * @param index Event index in the sequence
   * @param timestamp Event timestamp 
   * @param event_id Event ID
   * @param payload Event payload
   * @return Formatted event string
   */
  std::string 
  format_event(const event_data_t& event_data) const;

    // Data members
  config_strix m_config; 
};

} // namespace xrt_core::tools::xrt_smi

#endif // CONFIG_STRIX_H
