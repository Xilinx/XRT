// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "tools/common/OptionOptions.h"

class OO_EventTrace : public OptionOptions {
public:
  void execute( const SubCmdOptions &_options ) const override;
  void validate_args() const;

public:
  explicit OO_EventTrace(const std::string &_longName, bool _isHidden = false);

private:
  std::string m_device;
  std::string m_action;
  bool m_help;
};