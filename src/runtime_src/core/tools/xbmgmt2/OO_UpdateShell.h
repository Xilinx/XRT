// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __OO_UpdateShell_h_
#define __OO_UpdateShell_h_

#include "tools/common/OptionOptions.h"

class OO_UpdateShell : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_UpdateShell( const std::string &_longName, const std::string &_shortName, bool _isHidden = false);

 private:
  std::string m_device;
  std::string m_plp;
  bool m_help;
};

#endif
