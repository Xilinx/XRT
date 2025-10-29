// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef REPORT_EVENT_TRACE_H
#define REPORT_EVENT_TRACE_H

#include "core/common/query_requests.h"
#include "tools/common/Report.h"
#include "EventTrace.h"
#include <string>
#include <cstdint>
#include <vector>
#include <map>

namespace smi = xrt_core::tools::xrt_smi;

/**
 * @brief Report for firmware event trace information
 * 
 * This report provides information about firmware events
 * captured from the firmware. It displays chronological trace events with:
 * 
 * Event Trace Information:
 * - Timestamp: When the event occurred (in nanoseconds)
 * - Event ID: Unique identifier for the event type
 * - Event Name: Human-readable name for the event
 * - Category: Event category (NPU Scheduling, Mailbox, etc.)
 * - Payload: Event-specific data and arguments
 * - Context ID: Associated hardware context (if applicable)
 * 
 */
class ReportEventTrace : public Report {
public:
  /**
   * @brief Type aliases for compatibility with existing code
   */
  using event_trace_config = xrt_core::tools::xrt_smi::event_trace_config;

  /**
   * @brief Constructor for event trace report
   * 
   * Initializes the report with:
   * - Report name: "event-trace"
   */
  ReportEventTrace() 
    :  Report("event-trace", "Log to console firmware event trace information", true /*deviceRequired*/)
    {}

  /**
   * @brief Generate property tree representation of event trace data
   * 
   * Creates a structured property tree containing event trace information
   * suitable for JSON serialization and programmatic access.
   * 
   * @param device Target device to query
   * @param pt Property tree to populate with event data
   */
  void
  getPropertyTreeInternal(const xrt_core::device* device,
                         boost::property_tree::ptree& pt) const override;

  /**
   * @brief Generate property tree in 20202 format for compatibility
   * 
   * @param device Target device to query  
   * @param pt Property tree to populate
   */
  void
  getPropertyTree20202(const xrt_core::device* device,
                      boost::property_tree::ptree& pt) const override;

  /**
   * @brief Generate human-readable event trace report
   * 
   * Outputs formatted event trace information to the specified stream.
   * 
   * @param device Target device to query
   * @param pt Property tree data (unused for this report)
   * @param elements_filter Filter options (reserved for future use)
   * @param output Output stream for report text
   */

  void
  writeReport(const xrt_core::device* device,
             const boost::property_tree::ptree& pt,
             const std::vector<std::string>& elements_filter,
             std::ostream& output) const override;

private:
  /**
   * @brief Watch mode offset for continuous event trace streaming
   * 
   * This member variable tracks the current buffer offset when operating in watch mode.
   * It ensures that subsequent queries in watch mode continue from where the previous
   * query left off, providing seamless continuous event trace monitoring.
   * 
   * @note Reset to 0 at the beginning of each writeReport call
   */
  mutable uint64_t m_watch_mode_offset{0};

  /**
   * @brief Generate raw event trace data dump
   * 
   * @param dev XRT device to query for raw event trace buffer
   * @param is_watch True if operating in watch mode (continuous updates)  
   * @return std::string Raw binary trace data as string
   * 
   * This method provides direct access to the raw event trace buffer
   * without any parsing or formatting. Used as a fallback when:
   * - Configuration parsing fails
   * - User explicitly requests raw output with "--element raw"
   * 
   * @note Follows same pattern as ReportFirmwareLog::generate_raw_logs
   */
  std::string 
  generate_raw_logs(const xrt_core::device* dev, bool is_watch) const;

  /**
   * @brief Generate parsed and formatted event trace report
   * 
   * @param dev XRT device to query for event trace data
   * @param config Event trace configuration for parsing structure and events
   * @param is_watch True if operating in watch mode (continuous updates)
   * @param use_dummy True to use dummy data instead of device data
   * @return std::string Formatted trace output with parsed events and arguments
   * 
   * This method retrieves event trace data from the device and uses an internal
   * event_trace_parser class to parse and format it into human-readable output.
   * Similar pattern to ReportFirmwareLog with minimal overhead parsing.
   * 
   * @note Uses internal event_trace_parser class for efficient parsing logic
   */
  std::string 
  generate_parsed_logs(const xrt_core::device* dev,
                       const smi::event_trace_config& config,
                       bool is_watch) const;

  /**
   * @brief Validate event trace version compatibility
   * 
   * @param version Version pair from configuration file
   * @param device XRT device to check against
   * 
   * This method checks if the configuration file version matches the
   * firmware version to ensure proper event parsing compatibility.
   * Currently stubbed out pending driver version support.
   */
  void
  validate_version_compatibility(const std::pair<uint16_t, uint16_t>& version,
                                 const xrt_core::device* device) const;
};

#endif // REPORT_EVENT_TRACE_H
