// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __REPORT_EVENT_TRACE_H__
#define __REPORT_EVENT_TRACE_H__

#include "tools/common/Report.h"

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
   * @brief Constructor for event trace report
   * 
   * Initializes the report with:
   * - Report name: "event-trace"
   * - Description: "Log to console firmware event trace information"
   */
  ReportEventTrace() 
    :  Report("event-trace", "Log to console firmware event trace information", true /*deviceRequired*/) { };

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
};

#endif // __REPORT_EVENT_TRACE_H__
