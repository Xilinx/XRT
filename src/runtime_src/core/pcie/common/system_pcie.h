/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef SYSTEM_PCIE_H
#define SYSTEM_PCIE_H

#include "common/system.h"

namespace xrt_core {

class system_pcie : public system
{
public:
  void
  get_devices(boost::property_tree::ptree& pt) const;
};

} // xrt_core

#endif
