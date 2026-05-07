// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "tools/common/OptionOptions.h"

class OO_HardwareContextReset : public OptionOptions {
public:
  void execute(const SubCmdOptions& _options) const override;
  void validate_args() const;

  explicit OO_HardwareContextReset(const std::string& _longName, bool _isHidden = false);

private:
  std::string m_device;
  std::string m_reset_on_error;
  bool m_help;
};
