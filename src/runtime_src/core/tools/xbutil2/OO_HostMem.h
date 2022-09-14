// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __OO_HostMem_h_
#define __OO_HostMem_h_

#include "tools/common/OptionOptions.h"

class OO_HostMem : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_HostMem( const std::string &_longName, bool _isHidden = false);

 private:
  std::string m_device;
  std::string m_action;
  std::string m_size;
  bool m_help;
};

#endif
