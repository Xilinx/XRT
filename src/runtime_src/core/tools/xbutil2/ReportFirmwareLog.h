// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef REPORT_FIRMWARE_LOG_H
#define REPORT_FIRMWARE_LOG_H

#include "tools/common/Report.h"
#include "FirmwareLogConfig.h" // Include the header file for firmware_log_config

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
  void getPropertyTreeInternal(const xrt_core::device* dev, boost::property_tree::ptree& pt) const override;

  /**
   * @brief Get property tree representation for XRT 2020.2+ compatibility
   *
   * @param dev XRT device to query for firmware log information
   * @param pt Property tree to populate with firmware log data
   *
   * @note May have slightly different property tree structure for compatibility
   * @note Falls back to getPropertyTreeInternal implementation if no differences
   */
  void getPropertyTree20202(const xrt_core::device* dev, boost::property_tree::ptree& pt) const override;

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
  void writeReport(const xrt_core::device* device,
                   const boost::property_tree::ptree& pt,
                   const std::vector<std::string>& elements_filter,
                   std::ostream& output) const override;

public:
  /**
   * @brief Parse a single firmware log entry
   *
   * @param data_ptr Pointer to the firmware log data buffer
   * @param offset Offset within the buffer to start parsing
   * @param buf_size Total size of the data buffer
   * @param config Firmware log configuration object
   *
   * @return Vector of strings containing parsed log entry data
   *
   * This static method parses a single firmware log entry from the data buffer
   * and returns the extracted information as a vector of strings. The parsing
   * is based on the provided configuration object, which defines the structure
   * of the log entry.
   */
  static std::vector<std::string> parse_log_entry(const uint8_t* data_ptr, 
                                                  size_t offset, 
                                                  size_t buf_size, 
                                                  const xrt_core::tools::xrt_smi::firmware_log_config& config);
};

#endif // REPORT_FIRMWARE_LOG_H
