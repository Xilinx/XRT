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


#include "common/core_system.h"
#include "gen/version.h"


void xrt_core::system::get_xrt_info(boost::property_tree::ptree &_pt)
{
  _pt.put("build.version",   xrt_build_version);
  _pt.put("build.hash",      xrt_build_version_hash);
  _pt.put("build.date",      xrt_build_version_date);
  _pt.put("build.branch",    xrt_build_version_branch);

  //TODO
  // _pt.put("xocl",      driver_version("xocl"));
  // _pt.put("xclmgmt",   driver_version("xclmgmt"));
}


void xrt_core::system::get_os_info(boost::property_tree::ptree &_pt)
{
  // TODO
  _pt.put("windows", "");
}



