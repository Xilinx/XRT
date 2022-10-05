// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __OO_Hotplug_h_
#define __OO_Hotplug_h_

#include "tools/common/OptionOptions.h"

class OO_Hotplug : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_Hotplug( const std::string &_longName, bool _isHidden = false);

 private:
  std::string m_devices;
  std::string m_action;
  bool        m_help;
};

#endif
