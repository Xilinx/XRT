/**
 * Copyright (C) 2021-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdConfigure.h"
#include "tools/common/SubCmdConfigureInternal.h"
#include "OO_HostMem.h"
#include "OO_P2P.h"

std::vector<std::shared_ptr<OptionOptions>> SubCmdConfigureInternal::optionOptionsCollection = {
  std::make_shared<OO_HostMem>("host-mem"),
  std::make_shared<OO_P2P>("p2p"),
};

SubCmdConfigure::SubCmdConfigure(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree configurations)
    : SubCmdConfigureInternal(_isHidden, _isDepricated, _isPreliminary, true /*isUserDomain*/, configurations)
{
  // Empty
}
