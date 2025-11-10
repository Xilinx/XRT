// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "core/common/device.h"
#include "tools/common/OptionOptions.h"
#include "FirmwareLog.h"

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
  bool m_raw;
  mutable uint64_t m_watch_mode_offset;

  // Log generation methods for examine functionality
  std::string 
  generate_parsed_logs(const xrt_core::device* dev,
                       const smi::firmware_log_config& config,
                       bool is_watch) const;
  std::string 
  generate_raw_logs(const xrt_core::device* dev, 
                    bool is_watch) const;
};