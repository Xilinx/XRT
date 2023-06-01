// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __OO_Input_h_
#define __OO_Input_h_

#include "tools/common/OptionOptions.h"

class OO_Input : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_Input( const std::string &_longName, bool _isHidden = false);

 private:
  std::string m_device;
  std::string m_path;
  bool m_help;
};

#endif
