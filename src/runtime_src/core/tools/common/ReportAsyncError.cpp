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
#include "ReportAsyncError.h"
#include "core/common/device.h"
#include "core/common/time.h"
#include "core/common/api/error_int.h"
#include "core/include/experimental/xrt_error.h"
#include "core/include/xrt_error_code.h"
#include <boost/algorithm/string.hpp>

const static long unsigned NanoSecondsPerSecond = 1000000000;

boost::property_tree::ptree
populate_async_error(const xrt_core::device * device)
{
  boost::property_tree::ptree pt;
  boost::property_tree::ptree error_array;
  auto dhdl = xrtDeviceOpenFromXcl(device->get_device_handle());
  for (xrtErrorClass ecl = XRT_ERROR_CLASS_FIRST_ENTRY; ecl < XRT_ERROR_CLASS_LAST_ENTRY ; ecl = xrtErrorClass(ecl+1)) {
    xrtErrorCode errorCode = 0;
    uint64_t timestamp = 0;
    int rval = xrtErrorGetLast(dhdl, ecl, &errorCode, &timestamp);
    if (rval == 0 && errorCode && timestamp) {
      boost::property_tree::ptree _pt;
      boost::property_tree::ptree node;
      xrt_core::error_int::get_error_code_to_json(errorCode, _pt);
      node.put("time.epoch", timestamp);
      node.put("time.timestamp", xrt_core::timestamp(timestamp/NanoSecondsPerSecond));
      node.put("class", _pt.get<std::string>("class.string"));
      node.put("module", _pt.get<std::string>("module.string"));
      node.put("severity", _pt.get<std::string>("severity.string"));
      node.put("driver", _pt.get<std::string>("driver.string"));
      node.put("error_code.error_id", _pt.get<int>("number.code"));
      node.put("error_code.error_msg", _pt.get<std::string>("number.string"));
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

  //check if a valid report is generated
  boost::property_tree::ptree& pt_err = _pt.get_child("asynchronous_errors");
  if(pt_err.empty())
    return;

  _output << "Asynchronous Errors\n";
  boost::format fmt("  %-35s%-20s%-20s%-20s%-20s%-20s\n");
  _output << fmt % "Time" % "Class" % "Module" % "Driver" % "Severity" % "Error Code";
  for (auto& node : pt_err) {
    _output << fmt %  node.second.get<std::string>("time.timestamp")
               % node.second.get<std::string>("class") % node.second.get<std::string>("module")
               % node.second.get<std::string>("driver") % node.second.get<std::string>("severity")
               % node.second.get<std::string>("error_code.error_msg");
  }
  _output << std::endl;

}
