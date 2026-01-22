// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "core/common/device.h"
#include "tools/common/OptionOptions.h"
#include "FirmwareLog.h"
#include <optional>

namespace smi = xrt_core::tools::xrt_smi;

class OO_FirmwareLogExamine : public OptionOptions {
public:
  void execute( const SubCmdOptions &_options ) const override;
  void validate_args() const;

public:
  explicit OO_FirmwareLogExamine(const std::string &_longName, bool _isHidden = false);

private:
  std::string m_device;
  bool m_help;
  bool m_watch;
  bool m_status;
  std::optional<std::string> m_raw; 
  bool m_version;
  mutable uint64_t m_watch_mode_offset;

  // Main logging handler
  void
  handle_logging(const xrt_core::device* device) const;

  // Version handler
  void
  handle_version(const xrt_core::device* device) const;

  // Log generation methods for examine functionality
  std::string 
  generate_parsed_logs(const xrt_core::device* dev,
                       const smi::firmware_log_parser& parser,
                       bool is_watch) const;
  std::string 
  generate_raw_logs(const xrt_core::device* dev, 
                    bool is_watch) const;
};