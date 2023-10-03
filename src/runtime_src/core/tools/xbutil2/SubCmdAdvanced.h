// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdAdvanced_h_
#define __SubCmdAdvanced_h_

#include "tools/common/SubCmd.h"

class SubCmdAdvanced : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;

 public:
  SubCmdAdvanced(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree& configurations);
  virtual ~SubCmdAdvanced() {};

  private:
    bool m_help;
    std::string m_device;
};

#endif

