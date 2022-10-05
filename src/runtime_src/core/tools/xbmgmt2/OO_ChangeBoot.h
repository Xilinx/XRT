// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __OO_ChangeBoot_h_
#define __OO_ChangeBoot_h_

#include "tools/common/OptionOptions.h"

class OO_ChangeBoot : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_ChangeBoot( const std::string &_longName, const std::string &_shortName="", bool _isHidden = false);

 private:
  std::string m_device;
  std::string m_boot;
  bool        m_help;
};

#endif
