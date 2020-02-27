/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef SYSTEM_EDGE_H
#define SYSTEM_EDGE_H

#include "core/common/system.h"
#include "core/common/device.h"

namespace xrt_core {

/**
 * class system_edge - base class for system classes under edge
 *
 * All shim level libraries define a specific system and device class.
 * Share system code goes in this class.
 */  
class system_edge : public system
{
public:
  void
  get_devices(boost::property_tree::ptree& pt) const;
};

} // xrt_core

#endif /* SYSTEM_EDGE_H */
