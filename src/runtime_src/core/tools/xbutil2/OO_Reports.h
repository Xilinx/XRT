// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __OO_Reports_h_
#define __OO_Reports_h_

#include "tools/common/OptionOptions.h"

class OO_Reports : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_Reports( const std::string &_longName, bool _isHidden = false);

 private:
  std::string m_device;
  std::string m_action;
  bool m_help;
};

#endif
