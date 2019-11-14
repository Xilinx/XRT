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

#ifndef CORE_SYSTEM_H
#define CORE_SYSTEM_H

// Please keep eternal include file dependencies to a minimum
#include <boost/property_tree/ptree.hpp>

/**
 * This class is a library abstract class where:
 * 1) The header file is defined in core/common
 * 2) The body is implemented in a lower SSA directory. 
 *  
 * Note 
 *    The lower SSA directory for the given target (e.g., 
 *    pcie/linux, pcie/windows, edge/linux) is responsible for
 *    implementing the body and producing a xrt_core and
 *    xrt_core_static library.
 */
namespace xrt_core {
namespace system {
  void get_xrt_info(boost::property_tree::ptree &_pt);
  void get_os_info(boost::property_tree::ptree &_pt);
}
}

#endif /* CORE_SYSTEM_H */
