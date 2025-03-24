// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdConfigure_h_
#define __SubCmdConfigure_h_

#include "tools/common/SubCmd.h"
#include "tools/common/SubCmdConfigureInternal.h"

class SubCmdConfigure : public SubCmdConfigureInternal {
 public:
  SubCmdConfigure(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree& configurations);
};

#endif

