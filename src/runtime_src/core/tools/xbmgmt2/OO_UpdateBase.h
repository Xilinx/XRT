// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __OO_UpdateBase_h_
#define __OO_UpdateBase_h_

#include "tools/common/OptionOptions.h"

class OO_UpdateBase : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_UpdateBase( const std::string &_longName, const std::string &_shortName, bool _isHidden = false);

 private:
  std::string m_device;
  std::string m_action;
  std::string update;
  std::vector<std::string> image;
  std::string flashType;
  bool m_help;
};

#endif
