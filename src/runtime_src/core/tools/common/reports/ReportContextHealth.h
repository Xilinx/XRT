// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

// Please keep external include file dependencies to a minimum
#include "core/common/smi.h"
#include "tools/common/Report.h"

/**
 * @brief Report for hardware context health information
 * 
 * This report provides comprehensive information about the health status
 * of hardware contexts on XRT devices. It displays:
 * 
 * Hardware Context Information:
 * - Context ID: Unique identifier for each hardware context
 * - Transaction Operation Index: Current transaction operation being processed
 * - Context Program Counter: Current execution position in the context
 * - Fatal Error Type: Type of fatal error (if any) that occurred
 * - Fatal Error Exception Type: Specific exception type for fatal errors
 * - Fatal Error Exception PC: Program counter where fatal error occurred
 * - Fatal Error Application Module: Application module associated with error
 * 
 * Features:
 * - Supports context filtering by ID (ctx_id=<id> syntax)
 * - Watch mode support for real-time monitoring
 * 
 * 
 * Usage Examples:
 * @code
 * # Basic context health report
 * xrt-smi examine --report context-health
 * 
 * # Filter specific context
 * xrt-smi examine --report context-health --filter ctx_id=5
 * 
 * # Real-time monitoring
 * xrt-smi examine --report context-health --filter watch
 * 
 * @endcode
 */
class ReportContextHealth : public Report {
public:
  /**
   * @brief Constructor for context health report
   * 
   * Initializes the report with:
   * - Report name: "context-health"
   * - Description: "Log to console context health information"
   */
  ReportContextHealth() : Report("context-health", "Log to console context health information", true /*deviceRequired*/) { /*empty*/ };

  // Child methods that need to be implemented from Report base class
public:
  /**
   * @brief Get property tree representation of context health data
   * 
   * @param dev XRT device to query for context health information
   * @param pt Property tree to populate with context health data
   * 
   * This method implements the standard XRT report interface for
   * property tree generation. It queries the device using XRT's
   * device query system and populates the property tree with
   * structured context health information.
   * 
   * Property tree structure:
   * - context_health
   *   - context_count: Total number of contexts
   *   - contexts: Array of context objects
   *     - context_id: Context identifier
   *     - txn_op_idx: Transaction operation index  
   *     - ctx_pc: Context program counter
   *     - fatal_error_type: Fatal error type code
   *     - fatal_error_exception_type: Exception type code
   *     - fatal_error_exception_pc: Exception program counter
   *     - fatal_error_app_module: Application module identifier
   * 
   * @note Handles exceptions internally and reports errors in property tree
   * @note Compatible with JSON serialization for xrt-smi --format JSON
   */
  void 
  getPropertyTreeInternal(const xrt_core::device* dev, boost::property_tree::ptree& pt) const override;

  /**
   * @brief Get property tree representation for XRT 2020.2+ compatibility
   * 
   * @param dev XRT device to query for context health information  
   * @param pt Property tree to populate with context health data
   * 
   * @note May have slightly different property tree structure for compatibility
   * @note Falls back to getPropertyTreeInternal implementation if no differences
   */
  void 
  getPropertyTree20202(const xrt_core::device* dev, boost::property_tree::ptree& pt) const override;

  /**
   * @brief Write formatted context health report to output stream
   * 
   * @param device XRT device to query for current context health data
   * @param pt Property tree containing context health data (may be unused for direct queries)
   * @param elements_filter Vector of element filter strings for customization
   * @param output Output stream to write the formatted report
   * 
   * This method generates the human-readable formatted output for context health.
   * It supports:
   * 
   * Element Filters:
   * - "ctx_id=<id>" - Filter to show only specific context ID
   * - "watch" - Enable real-time watch mode with 1-second updates
   * 
   * @note Integrates with smi_watch_mode for real-time monitoring
   * @note Thread-safe implementation for watch mode usage
   */
  void writeReport(const xrt_core::device* device, 
                   const boost::property_tree::ptree& pt, 
                   const std::vector<std::string>& elements_filter, 
                   std::ostream& output) const override;

protected:
  /**
   * @brief Virtual function to generate hardware-specific context health report
   * 
   * @param dev XRT device to query for context health information
   * @param context_pid_pairs Vector of context ID and PID pairs
   * @param context_ids Vector of context IDs for filtering
   * @return Formatted context health report string
   * 
   * This pure virtual function must be implemented by derived classes
   * to provide hardware-specific context health report generation.
   */
  virtual std::string 
  generate_report(const xrt_core::device* dev,
                  const std::vector<std::pair<uint64_t, uint64_t>>& context_pid_pairs,
                  const std::vector<uint64_t>& context_ids) const;

private:
  /**
   * @brief Create appropriate context health reporter based on hardware type
   * 
   * @param hw_type Hardware type detected from the device
   * @return Unique pointer to the appropriate reporter implementation
   */
  std::unique_ptr<ReportContextHealth>
  create_reporter(xrt_core::smi::smi_hardware_config::hardware_type hw_type) const;

  /**
   * @brief Parse comma-separated values from input string
   * 
   * @param input Input string containing comma-separated values
   * @return Vector of parsed uint64_t values
   */
  std::vector<uint64_t> 
  parse_values(const std::string& input) const;

  /**
   * @brief Parse context IDs from filter elements
   * 
   * @param elements_filter Vector of filter elements
   * @return Vector of context IDs
   */
  std::vector<uint64_t> 
  parse_context_ids(const std::vector<std::string>& elements_filter) const;

  /**
   * @brief Parse context ID and PID pairs from filter elements
   * 
   * @param elements_filter Vector of filter elements
   * @return Vector of context ID and PID pairs
   */
  std::vector<std::pair<uint64_t, uint64_t>> 
  parse_context_pid_pairs(const std::vector<std::string>& elements_filter) const;
};

/**
 * @brief Hardware-specific context health report for STRX/KRCK platforms
 * 
 * Provides STRX/KRCK specific context health reporting with appropriate
 * field mappings and table headers for these hardware types.
 */
class ctx_health_strx : public ReportContextHealth {
public:
  ctx_health_strx() : ReportContextHealth() {}

protected:
  std::string 
  generate_report(const xrt_core::device* dev,
                  const std::vector<std::pair<uint64_t, uint64_t>>& context_pid_pairs,
                  const std::vector<uint64_t>& context_ids) const override;
};

/**
 * @brief Hardware-specific context health report for NPU3 platforms
 * 
 * Provides NPU3 specific context health reporting with appropriate
 * field mappings and table headers optimized for NPU3 hardware.
 */
class ctx_health_npu3 : public ReportContextHealth {
public:
  ctx_health_npu3() : ReportContextHealth() {}

protected:
  std::string 
  generate_report(const xrt_core::device* dev,
                  const std::vector<std::pair<uint64_t, uint64_t>>& context_pid_pairs,
                  const std::vector<uint64_t>& context_ids) const override;
};
