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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportFirewall.h"

void
ReportFirewall::getPropertyTreeInternal( const xrt_core::device * _pDevice,
                                         boost::property_tree::ptree &_pt) const
{
  // Defer to the 20201 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20201(_pDevice, _pt);
}

void 
ReportFirewall::getPropertyTree20201( const xrt_core::device * /*_pDevice*/,
                                       boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  pt.put("Description","Firewall Information");

  // There can only be 1 root node
  _pt.add_child("firewall", pt);
}


void 
ReportFirewall::writeReport(const xrt_core::device * /*_pDevice*/,
                            const std::vector<std::string> & /*_elementsFilter*/,
                            std::iostream & _output) const
{
  _output << "ReportFirewall - Hello world\n";
}


