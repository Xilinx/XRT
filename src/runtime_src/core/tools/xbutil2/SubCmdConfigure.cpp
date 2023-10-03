// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdConfigure.h"
#include "tools/common/SubCmdConfigureInternal.h"
#include "OO_HostMem.h"
#include "OO_P2P.h"
#include "OO_Performance.h"

std::vector<std::shared_ptr<OptionOptions>> SubCmdConfigureInternal::optionOptionsCollection = {
  std::make_shared<OO_HostMem>("host-mem"),
  std::make_shared<OO_P2P>("p2p"),
  std::make_shared<OO_Performance>("performance"),
};

SubCmdConfigure::SubCmdConfigure(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree& configurations)
    : SubCmdConfigureInternal(_isHidden, _isDepricated, _isPreliminary, true /*isUserDomain*/, configurations)
{
  // Empty
}
