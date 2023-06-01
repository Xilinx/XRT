// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __OO_Retention_h_
#define __OO_Retention_h_

#include "tools/common/OptionOptions.h"

class OO_Retention : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_Retention( const std::string &_longName, bool _isHidden = false);

 private:
  std::string m_device;
  std::string m_retention;
  bool m_help;
};

#endif
