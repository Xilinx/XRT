// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef REPORT_FIRMWARE_LOG_H
#define REPORT_FIRMWARE_LOG_H

#include "tools/common/Report.h"
#include "FirmwareLog.h" // Include the header file for firmware_log_config
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace smi = xrt_core::tools::xrt_smi;
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
    : Report("firmware-log", "Log to console firmware log information", true /*deviceRequired*/),
      m_watch_mode_offset(0) {}

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

private:
  /**
   * @brief Watch mode offset for continuous log streaming
   * 
   * This member variable tracks the current buffer offset when operating in watch mode.
   * It ensures that subsequent queries in watch mode continue from where the previous
   * query left off.
   * 
   * @note Reset to 0 at the beginning of each writeReport call
   */
  mutable uint64_t m_watch_mode_offset;

  /**
   * @brief Generate raw firmware log data dump
   * 
   * @param dev XRT device to query for raw firmware log buffer
   * @param is_watch True if operating in watch mode (continuous updates)  
   * @return std::string Raw binary log data as string
   * 
   * This method provides direct access to the raw firmware log buffer
   * without any parsing or formatting. Used as a fallback when:
   * - Configuration parsing fails
   * - User explicitly requests raw output with "--element raw"
   * 
   * @note Updates m_watch_mode_offset for continuous streaming
   */
  std::string 
  generate_raw_logs(const xrt_core::device* dev, bool is_watch) const;

  /**
   * @brief Generate parsed and formatted firmware log report
   * 
   * @param dev XRT device to query for firmware log data
   * @param config Firmware log configuration for parsing structure and enums
   * @param is_watch True if operating in watch mode (continuous updates)
   * @return std::string Formatted log table with parsed fields and messages
   * 
   * This method retrieves firmware log data from the device and uses the provided
   * configuration to parse and format it into a human-readable table. 
   * 
   * @note Uses firmware_log_parser class for actual parsing logic
   */
  std::string 
  generate_parsed_logs(const xrt_core::device* dev,
                       const smi::firmware_log_config& config,
                       bool is_watch) const;

};

#endif // REPORT_FIRMWARE_LOG_H
