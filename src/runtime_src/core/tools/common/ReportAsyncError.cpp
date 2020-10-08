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
#include <ctime>
#include "ReportAsyncError.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include "core/include/xrt.h"
#include "core/include/experimental/xrt_error.h"
#include "core/include/xrt_error_code.h"
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
namespace qr = xrt_core::query;

boost::property_tree::ptree
populate_async_error(const xrt_core::device * device)
{
  boost::property_tree::ptree pt;
  boost::property_tree::ptree error_array;
  auto dhdl = xrtDeviceOpenFromXcl(device->get_device_handle());
  for (xrtErrorClass ecl = XRT_ERROR_CLASS_FIRST_ENTRY; ecl < XRT_ERROR_CLASS_UNKNOWN; ecl = xrtErrorClass(ecl+1)) {
    xrtErrorCode errorCode = 0;
    uint64_t timestamp = 0;
    int rval = xrtErrorGetLast(dhdl, ecl, &errorCode, &timestamp);
    /**
     * In case of no error for given class, errorCode and timestamp will be zero,
     * but zero errorCode is valid error, so checking for only timestamp.
     */
    if (rval == 0 && errorCode && timestamp) {
      size_t len = 0;
      if (xrtErrorGetJson(dhdl, errorCode, nullptr, 0, &len))
        continue;
      char buf[len];
      if (xrtErrorGetJson(dhdl, errorCode, buf, len, nullptr))
        continue;
      std::stringstream ss(buf);
      boost::property_tree::ptree _pt;
      boost::property_tree::read_json(ss, _pt);
      boost::property_tree::ptree node;
      node.put("timestamp", timestamp);
      node.put("class.code", _pt.get<int>("class.code"));
      node.put("class.string", _pt.get<std::string>("class.string"));
      node.put("module.code", _pt.get<int>("module.code"));
      node.put("module.string", _pt.get<std::string>("module.string"));
      node.put("severity.code", _pt.get<int>("severity.code"));
      node.put("severity.string", _pt.get<std::string>("severity.string"));
      node.put("driver.code", _pt.get<int>("driver.code"));
      node.put("driver.string", _pt.get<std::string>("driver.string"));
      node.put("number.code", _pt.get<int>("number.code"));
      node.put("number.string", _pt.get<std::string>("number.string"));
      error_array.push_back(std::make_pair("", node));
    }
  }
  pt.add_child("errors", error_array);

  return error_array;
}

void
ReportAsyncError::getPropertyTreeInternal( const xrt_core::device * _pDevice, 
                                              boost::property_tree::ptree &_pt) const
{
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportAsyncError::getPropertyTree20202( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  _pt.add_child("asynchronous_errors", populate_async_error(_pDevice));
}

void 
ReportAsyncError::writeReport( const xrt_core::device * _pDevice,
                                  const std::vector<std::string> & /*_elementsFilter*/, 
                                  std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);

  _output << "Asynchronous Errors\n";
  _output << boost::format("  %-35s%-20s%-20s%-20s%-20s%-20s\n") % "Time" % "Class" % "Module" % "Driver" % "Severity" % "Error Code";
  for (auto& node : _pt.get_child("asynchronous_errors")) {
    _output << boost::format("  %-35s%-20s%-20s%-20s%-20s%-20s\n") % boost::posix_time::from_time_t(node.second.get<long unsigned>("timestamp")/1000000000)
               % node.second.get<std::string>("class.string") % node.second.get<std::string>("module.string")
               % node.second.get<std::string>("driver.string") % node.second.get<std::string>("severity.string")
               % node.second.get<std::string>("number.string");
  }
  _output << std::endl;
  
}
