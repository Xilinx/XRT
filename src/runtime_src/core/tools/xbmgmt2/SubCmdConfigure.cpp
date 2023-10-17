// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdConfigure.h"
#include "tools/common/OptionOptions.h"
#include "tools/common/SubCmdConfigureInternal.h"
#include "OO_Input.h"
#include "OO_Retention.h"

std::vector<std::shared_ptr<OptionOptions>> SubCmdConfigureInternal::optionOptionsCollection = {
  std::make_shared<OO_Input>("input"),
  std::make_shared<OO_Retention>("retention"),
};

SubCmdConfigure::SubCmdConfigure(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree configurations)
    : SubCmdConfigureInternal(_isHidden, _isDepricated, _isPreliminary, false /*Not isUserDomain*/, configurations)
{
  // Empty
}
