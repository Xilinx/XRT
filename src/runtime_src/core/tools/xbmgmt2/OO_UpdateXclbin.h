// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __OO_UpdateXclbin_h_
#define __OO_UpdateXclbin_h_

#include "tools/common/OptionOptions.h"

class OO_UpdateXclbin : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_UpdateXclbin( const std::string &_longName, const std::string &_shortName="", bool _isHidden = false);

 private:
  std::string m_device;
  std::string xclbin;
  bool m_help;
};

#endif
