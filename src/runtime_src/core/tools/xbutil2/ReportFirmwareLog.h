// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef REPORT_FIRMWARE_LOG_H
#define REPORT_FIRMWARE_LOG_H

#include "tools/common/Report.h"
#include "FirmwareLogConfig.h" // Include the header file for firmware_log_config
#include "tools/common/Table2D.h"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace xrt_core::tools::xrt_smi {

// Forward declarations
class firmware_log_config;

/**
 * @brief Firmware log parser for XRT devices
 * 
 * This class provides an interface for parsing firmware log data.
 * It encapsulates all parsing logic. The parser is configured with a firmware_log_config object
 * that defines the structure and format of the log entries.
 */
class firmware_log_parser {
public:
  /**
   * @brief Construct a new firmware log parser
   * 
   * @param config Firmware log configuration object containing structure definitions,
   *               field layouts, and enumeration mappings
   */
  explicit 
  firmware_log_parser(firmware_log_config config);

  /**
   * @brief Parse firmware log buffer and generate formatted table
   * 
   * @param data_ptr Pointer to the firmware log data buffer
   * @param buf_size Total size of the data buffer in bytes
   * @return Table2D Formatted table containing parsed log entries
   * 
   * This method processes the entire firmware log buffer, parsing each entry
   * according to the configured structure.
   */
  Table2D 
  parse(const uint8_t* data_ptr, size_t buf_size) const;

private:
  /**
   * @brief Extract field value from bit-packed firmware log header
   * 
   * @param data_ptr Pointer to raw data buffer
   * @param byte_offset Starting byte offset
   * @param bit_offset Starting bit offset
   * @param bit_width Number of bits to extract
   * @return uint64_t Extracted field value
   */
  uint64_t 
  extract_value(const uint8_t* data_ptr, 
                size_t byte_offset, 
                size_t bit_offset, 
                size_t bit_width) const;

  /**
   * @brief Format field value with enum resolution if applicable
   * 
   * @param field Field information containing name and enumeration
   * @param value Raw field value
   * @return std::string Formatted field value with enum name if applicable
   */
  std::string 
  format_value(const firmware_log_config::field_info& field, 
               uint64_t value) const;

  /**
   * @brief Parse message data from log entry payload
   * 
   * @param data_ptr Pointer to the raw data buffer
   * @param msg_offset Offset where message data begins
   * @param argc Argument count field value (unused but kept for compatibility)
   * @param buf_size Total buffer size for bounds checking
   * @return std::string Parsed message with trailing newlines removed
   * 
   * This method extracts null-terminated string messages from log entries,
   * with newline cleanup.
   */
  std::string 
  parse_message(const uint8_t* data_ptr, 
                size_t msg_offset, 
                size_t buf_size) const;

  /**
   * @brief Parse a complete log entry (header + message)
   * 
   * @param data_ptr Pointer to the raw data buffer
   * @param offset Offset where log entry begins
   * @param buf_size Total buffer size for bounds checking
   * @return std::vector<std::string> Vector of parsed field values and message
   * 
   * This method parses a complete log entry by:
   * 1. Extracting all header fields according to structure definition
   * 2. Parsing the message payload
   * 3. Returning organized field data for table display
   */
  std::vector<std::string> 
  parse_entry(const uint8_t* data_ptr, size_t offset, 
                  size_t buf_size) const;

  /**
   * @brief Generate table headers based on log structure configuration
   * 
   * @return std::vector<Table2D::HeaderData> Vector of table headers with justification
   * 
   */
  std::vector<Table2D::HeaderData> 
  get_table_headers() const;

  /**
   * @brief Calculate entry size for buffer traversal
   * 
   * @param argc Argument count field value
   * @param format Log format field value (full vs concise)
   * @return uint32_t Total size of log entry in bytes
   * 
   */
  uint32_t 
  calculate_entry_size(uint32_t argc, uint32_t format) const;

private:
  firmware_log_config m_config;  
  firmware_log_config::structure_info m_header; 
  uint32_t m_header_size; // Size of log entry header in bytes
  
  // Field indices computed from config
  std::unordered_map<std::string, size_t> m_field_indices;

  // Column headers mapping for display
  std::unordered_map<std::string, std::string> m_columns;

  /**
   * @brief Create field indices map from config
   */
  static std::unordered_map<std::string, size_t> 
  create_field_indices(const firmware_log_config& config);
};

} // namespace xrt_core::tools::xrt_smi

/**
 * @brief Report for firmware log information
 * 
 * This report provides information about firmware logs on XRT devices.
 * It displays:
 * - Timestamp
 * - Log Level
 * - Message
 *
 * Usage Examples:
 * @code
 * # Basic firmware log report
 * xrt-smi examine --report firmware-log
 *
 * @endcode
 */
class ReportFirmwareLog : public Report {
public:
  ReportFirmwareLog() 
    : Report("firmware-log", "Log to console firmware log information", true /*deviceRequired*/) { };

  // Child methods that need to be implemented from Report base class
public:
  /**
   * @brief Get property tree representation of firmware log data
   *
   * @param dev XRT device to query for firmware log information
   * @param pt Property tree to populate with firmware log data
   *
   * This method implements the standard XRT report interface for
   * property tree generation. It queries the device using XRT's
   * device query system and populates the property tree with
   * structured firmware log information.
   *
   * Property tree structure:
   * - firmware_log
   *   - log_count: Total number of log entries
   *   - logs: Array of log objects
   *     - timestamp: Log timestamp
   *     - level: Log level
   *     - message: Log message
   *
   * @note Handles exceptions internally and reports errors in property tree
   * @note Compatible with JSON serialization for xrt-smi --format JSON
   */
  void 
  getPropertyTreeInternal(const xrt_core::device* dev, boost::property_tree::ptree& pt) const override;

  /**
   * @brief Get property tree representation for XRT 2020.2+ compatibility
   *
   * @param dev XRT device to query for firmware log information
   * @param pt Property tree to populate with firmware log data
   *
   * @note May have slightly different property tree structure for compatibility
   * @note Falls back to getPropertyTreeInternal implementation if no differences
   */
  void 
  getPropertyTree20202(const xrt_core::device* dev, boost::property_tree::ptree& pt) const override;

  /**
   * @brief Write formatted firmware log report to output stream
   *
   * @param device XRT device to query for current firmware log data
   * @param pt Property tree containing firmware log data (may be unused for direct queries)
   * @param elements_filter Vector of element filter strings for customization
   * @param output Output stream to write the formatted report
   *
   * This method generates the human-readable formatted output for firmware logs.
   * It supports:
   *
   * Element Filters:
   * - "log_level=<level>" - Filter to show only specific log level
   * - "watch" - Enable real-time watch mode with 1-second updates
   *
   * @note Integrates with smi_watch_mode for real-time monitoring
   * @note Thread-safe implementation for watch mode usage
   */
  void 
  writeReport(const xrt_core::device* device,
              const boost::property_tree::ptree& pt,
              const std::vector<std::string>& elements_filter,
              std::ostream& output) const override;


};

#endif // REPORT_FIRMWARE_LOG_H
