// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// System - Include Files
#include <map>

// Local - Include Files
#include "ReportPlatforms.h"
#include "ReportAlveoPlatform.h"
#include "ReportRyzenPlatform.h"
#include "core/common/info_platform.h"
#include "core/common/query_requests.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>
void
ReportPlatforms::getPropertyTreeInternal( const xrt_core::device * dev, 
                                              boost::property_tree::ptree &pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void 
ReportPlatforms::getPropertyTree20202( const xrt_core::device * dev, 
                                           boost::property_tree::ptree &pt) const
{
  // There can only be 1 root node
  pt = xrt_core::platform::platform_info(dev);
}

void 
ReportPlatforms::writeReport( const xrt_core::device* _pDevice,
                              const boost::property_tree::ptree& _pt,
                              const std::vector<std::string>& _elementsFilter,
                              std::ostream & _output) const
{
  const auto device_class = xrt_core::device_query_default<xrt_core::query::device_class>(_pDevice, xrt_core::query::device_class::type::alveo);
  switch (device_class) {
  case xrt_core::query::device_class::type::alveo:
  {
    ReportAlveoPlatform alveo_report;
    alveo_report.writeReport(_pDevice, _pt, _elementsFilter, _output);
    break;
  }
  case xrt_core::query::device_class::type::ryzen:
  {
    ReportRyzenPlatform ryzen_report;
    ryzen_report.writeReport(_pDevice, _pt, _elementsFilter, _output);
    break;
  }
  default:
    XBUtilities::throw_cancel(boost::format("Invalid device class type: %d") % xrt_core::query::device_class::enum_to_str(device_class));
  }
}
