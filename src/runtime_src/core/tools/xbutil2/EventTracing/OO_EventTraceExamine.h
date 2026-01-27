// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "core/common/device.h"
#include "tools/common/OptionOptions.h"
#include "EventTraceBase.h"
#include <optional>

namespace smi = xrt_core::tools::xrt_smi;

class OO_EventTraceExamine : public OptionOptions {
public:
  void execute( const SubCmdOptions &_options ) const override;

public:
  explicit OO_EventTraceExamine(const std::string &_longName, bool _isHidden = false);

private:
  std::string m_device;
  bool m_help;
  bool m_watch;
  std::optional<std::string> m_raw;
  bool m_version;
  bool m_status;
  mutable uint64_t m_watch_mode_offset;

  // Handler methods for different modes
  void
  handle_version(const xrt_core::device* device) const;

  void
  handle_status(const xrt_core::device* device) const;

  void
  handle_logging(const xrt_core::device* device) const;

  // Log generation methods for examine functionality
  std::string 
  generate_parsed_logs(const xrt_core::device* dev,
                       const std::unique_ptr<smi::event_trace_parser>& parser,
                       bool is_watch) const;
  std::string 
  generate_raw_logs(const xrt_core::device* dev, 
                    bool is_watch) const;

  // Format table header for trace events
  std::string 
  add_header() const;

  // Display raw version as 4 bytes with LSB first
  // This is required by firmware to consume event-logs
  void 
  dump_raw_version(const xrt_core::device* device) const;
};