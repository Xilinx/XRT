// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdProgram_h_
#define __SubCmdProgram_h_

#include "tools/common/SubCmd.h"

class SubCmdProgram : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;

 public:
  SubCmdProgram(bool _isHidden, bool _isDepricated, bool _isPreliminary);
  virtual ~SubCmdProgram() {};

 private:
  bool m_help;
  std::string m_device;
};

#endif

