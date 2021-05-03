/**
 * Copyright (C) 2021 Xilinx, Inc
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
#include "ReportQspiStatus.h"
#include "core/common/query_requests.h"

namespace qr = xrt_core::query;

void
ReportQspiStatus::getPropertyTreeInternal(const xrt_core::device * device, 
                                              boost::property_tree::ptree &pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(device, pt);
}

void 
ReportQspiStatus::getPropertyTree20202( const xrt_core::device * device, 
                                           boost::property_tree::ptree &pt) const
{
  auto qspi_stat = xrt_core::device_query<qr::xmc_qspi_status>(device);
  boost::property_tree::ptree ptree;
  ptree.put("primary", std::get<0>(qspi_stat));
  ptree.put("recovery", std::get<1>(qspi_stat));
  
  // There can only be 1 root node
  pt.add_child("qspi_wp_status", ptree);
}

void 
ReportQspiStatus::writeReport( const xrt_core::device* /*_pDevice*/,
                               const boost::property_tree::ptree& _pt, 
                               const std::vector<std::string>& /*_elementsFilter*/, 
                               std::ostream & _output) const
{
  boost::property_tree::ptree ptEmpty;
  const boost::property_tree::ptree& ptree = _pt.get_child("qspi_wp_status", ptEmpty);

  _output << "QSPI write protection status" << std::endl;
  _output << boost::format("  %-23s: %s\n") % "Primary" % ptree.get<std::string>("primary");
  _output << boost::format("  %-23s: %s\n") % "Recovery" % ptree.get<std::string>("recovery");
  _output << std::endl;
}
