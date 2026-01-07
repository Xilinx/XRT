// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "tools/common/OptionOptions.h"

class OO_ContextHealth : public OptionOptions {
public:
  void execute( const SubCmdOptions &_options ) const override;

public:
  explicit OO_ContextHealth(const std::string &_longName, bool _isHidden = false);

private:
  std::string m_device;
  bool m_help;
  bool m_watch;
  std::string m_ctx_id_list;
  std::string m_pid_list;
};

