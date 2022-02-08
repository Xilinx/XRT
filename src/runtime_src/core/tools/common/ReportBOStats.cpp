/**
 * Copyright (C) 2022 Xilinx, Inc
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
#include "ReportBOStats.h"
#include "XBUtilities.h"

// 3rd Party Library - Include Files
#include <string>
#include <boost/format.hpp>
#include <boost/range/as_array.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace xq = xrt_core::query;

void
ReportBOStats::getPropertyTreeInternal( const xrt_core::device * pDevice,
                                     boost::property_tree::ptree & pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(pDevice, pt);
}

void
ReportBOStats::getPropertyTree20202( const xrt_core::device * pDevice,
                                  boost::property_tree::ptree & pt) const
{
  boost::property_tree::ptree pt1;
  boost::property_tree::ptree pt2_list;

  try {
    pt1.put("device", xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(pDevice)));

    auto mem_stat_char = xrt_core::device_query<xrt_core::query::memstat>(pDevice);
    std::vector<std::string> mem_stat;

    //Convert vector of char from query cmd into vector of strings separated by null char separated by null char
    boost::split(mem_stat, mem_stat_char, boost::is_any_of(boost::as_array("\n\0")));
    /* Content of mem_stat now will be as below:
        [UNUSED] bank0@0x004000000000 (16384MB): 0KB 0BOs
        [IN-USE] bank1@0x005000000000 (16384MB): 0KB 0BOs
        [UNUSED] bank2@0x006000000000 (16384MB): 0KB 0BOs
        [UNUSED] bank3@0x007000000000 (16384MB): 0KB 0BOs
        [UNUSED] PLRAM[0]@0x003000000000 (0MB): 0KB 0BOs
        [UNUSED] PLRAM[1]@0x003000400000 (0MB): 0KB 0BOs
        [UNUSED] PLRAM[2]@0x003000600000 (0MB): 0KB 0BOs
        [IN-USE] HOST[0]@0x002000000000 (16384MB): 0KB 0BOs
        [== BO Stats Below ==] NA@0x000000000000 (0MB): 0KB 0BOs
        [Regular] 0KB 0BOs
        [UserPointer] 0KB 0BOs
        [P2P] 0KB 0BOs
        [DeviceOnly] 0KB 0BOs
        [Imported] 0KB 0BOs
        [ExecBuf] 0KB 0BOs
        [CMA] 0KB 0BOs
    */
    bool found = false;

    //Skip DDR usage info by looking for line with string "BO Stats Below"
    //Following that are BO stats. BO type, size in KB and num of BOs fields separated by blank space
    for (auto& line: mem_stat) {
      if (line.empty())
        continue;

      if (found) {
        //Capture BOStat fields separated by blank space
        std::vector<std::string> bo_info;
        boost::split(bo_info, line, boost::is_any_of("\t "));
        //Expected fileds are BO type, size in KB and Num of BOs
        if (bo_info.size() != 3)
          throw xrt_core::error((boost::format("ERROR: Unexpected format in BO Stats. Line: %s") % line).str());
        
        boost::property_tree::ptree bo_pt;
        bo_pt.put("type", bo_info[0].substr(1, bo_info[0].length()-2));
        bo_pt.put("mem_used", bo_info[1].substr(0, bo_info[1].length()-2));
        bo_pt.put("mem_used_unit", bo_info[1].substr(bo_info[1].length()-2, 2));
        bo_pt.put("count", bo_info[2].substr(0, bo_info[2].length()-3));
        //pt1.add_child("bo", bo_pt);
        pt2_list.push_back(std::make_pair("", bo_pt));
        continue;
      }

      //Skip DDR usage stats at begining by looking for beloe string
      if (line.find("BO Stats Below") != std::string::npos)
        found = true;
    }
  } catch(const xrt_core::error& e) {
    std::cerr << e.what() << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  pt1.add_child("all_bos", pt2_list);
  // There can only be 1 root node
  pt.add_child("bostats", pt1);
}


void
ReportBOStats::writeReport(const xrt_core::device* /*pDevice*/,
                        const boost::property_tree::ptree& pt,
                        const std::vector<std::string>& /*elementsFilter*/,
                        std::ostream & output) const
{
  boost::format entfmt("BO type: %-11s, Mem usage(%s): %-8s, BO count: %-5s");


  boost::property_tree::json_parser::write_json(output, pt, true);

  for(auto& kv : pt.get_child("bostats.all_bos")) {
    //if (kv.first == "bo") {
      const boost::property_tree::ptree& v = kv.second;

      output << boost::str(entfmt 
        % v.get<std::string>("type")
        % v.get<std::string>("mem_used_unit")
        % v.get<std::string>("mem_used")
        % v.get<std::string>("count"));
      output << std::endl;
    //}
  }
  output << std::endl;
}
