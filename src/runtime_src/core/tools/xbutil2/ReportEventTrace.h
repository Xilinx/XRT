// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef REPORT_EVENT_TRACE_H
#define REPORT_EVENT_TRACE_H

#include "tools/common/Report.h"
#include "EventTraceConfig.h"
namespace smi = xrt_core::tools::xrt_smi;

/**
 * @brief Report for firmware event trace information
 * 
 * This report provides comprehensive information about firmware event traces
 * captured from the device. It displays chronological trace events with:
 * 
 * Event Trace Information:
 * - Timestamp: When the event occurred (in nanoseconds)
 * - Event ID: Unique identifier for the event type
 * - Event Name: Human-readable name for the event
 * - Category: Event category (NPU Scheduling, Mailbox, etc.)
 * - Payload: Event-specific data and arguments
 * - Context ID: Associated hardware context (if applicable)
 * 
 * Event Categories Include:
 * - NPU Scheduling: Frame start/done, preemption events
 * - Mailbox: Message processing, context management
 * - Clock/Power: Power gating, clock management
 * - Errors: Async errors, fatal errors
 * - PDI Load: Program download and initialization
 * - L2 Memory: Save/restore operations
 * 
 */
class ReportEventTrace : public Report {
public:
  /**
   * @brief Event trace data structure (must match device.cpp implementation)
   */
  struct trace_event {
    uint64_t timestamp;    // Simulated timestamp
    uint16_t event_id;     // Event ID from trace_events.h
    uint64_t payload;      // Event payload/arguments
  };

  /**
   * @brief Type aliases for compatibility with existing code
   */
  using event_trace_config = xrt_core::tools::xrt_smi::event_trace_config;
  using event_record = xrt_core::tools::xrt_smi::event_record;

  /**
   * @brief Constructor for event trace report
   * 
   * Initializes the report with:
   * - Report name: "event-trace"
   * - Description: "Log to console firmware event trace information"
   */
  ReportEventTrace() 
    :  Report("event-trace", "Log to console firmware event trace information", true /*deviceRequired*/),
       m_watch_mode_offset(0) { };

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
  mutable uint64_t m_watch_mode_offset;

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
   * @note Updates m_watch_mode_offset for continuous streaming
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
   * @return std::string Formatted trace table with parsed events and arguments
   * 
   * This method retrieves event trace data from the device and uses the provided
   * configuration to parse and format it into a human-readable table. 
   * 
   * @note Uses xrt_core::tools::xrt_smi::event_trace_config class for actual parsing logic
   */
  std::string 
  generate_parsed_logs(const xrt_core::device* dev,
                      const xrt_core::tools::xrt_smi::event_trace_config& config,
                      bool is_watch,
                      bool use_dummy = false) const;

  /**
   * @brief Generate dummy event trace data for testing
   * 
   * @param log_buffer Buffer to fill with dummy trace data
   * 
   * This method creates simulated event trace data with realistic event IDs
   * and payloads for testing purposes. Each call generates unique data
   * with incrementing counters to simulate real device behavior.
   */
  void 
  generate_dummy_event_trace_data(xrt_core::query::firmware_debug_buffer& log_buffer) const;

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
