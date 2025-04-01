// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __OO_Preemption_h_
#define __OO_Preemption_h_

#include "tools/common/OptionOptions.h"

class OO_Preemption : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;
  void validate_args() const;

 public:
  OO_Preemption( const std::string &_longName, bool _isHidden = true);

 private:
  std::string m_device;
  std::string m_action;
  std::string m_type;
  bool m_help;
};

#endif
