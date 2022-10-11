/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#define XDP_SOURCE

#include <chrono>
#include <ctime>
#include <cstring>
#include <boost/property_tree/ptree.hpp>

#include "core/common/system.h"

#include "xdp/profile/plugin/vp_base/utility.h"

#ifdef _WIN32
#pragma warning(disable : 4996)
/* Disable warning for use of "localtime" and "getenv" */
#endif

namespace xdp {

  std::string getCurrentDateTime()
  {
    auto time = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    struct tm *p_tstruct = std::localtime(&time);
    if(p_tstruct) {
        char buf[80] = {0};
        strftime(buf, sizeof(buf), "%Y-%m-%d %X", p_tstruct);
        return std::string(buf);
    }
    return std::string("0000-00-00 0000");
  }

  const char* getToolVersion()
  {
    return "2022.2" ;
  }

  std::string getXRTVersion()
  {
    static std::string version = "" ;
    if (version != "") return version ;

    boost::property_tree::ptree xrtInfo ;
    xrt_core::get_xrt_build_info(xrtInfo) ;

    version = xrtInfo.get<std::string>("version", "N/A") ;

    return version ;
  }

  // This function can only be called after the system singleton has
  //  been created on the XRT side.  This means we cannot call it in any
  //  plugin constructor.
  bool isEdge()
  {
    boost::property_tree::ptree pt ;
    xrt_core::get_xrt_info(pt) ;

    try {
      for (boost::property_tree::ptree::value_type& info : pt.get_child("drivers")) {
        try {
          std::string str = info.second.get<std::string>("name");
          if(0 == str.compare("zocl")) {
            return true;
          }
        } catch (const boost::property_tree::ptree_error&) {
          continue;
        }
      }
    } catch (const boost::property_tree::ptree_error&) {
      return false;
    }
    return false;
  }

  Flow getFlowMode()
  {
    static Flow mode = UNKNOWN ;
    static bool initialized = false ;

    if (initialized) return mode ;

    initialized = true ;
    const char* envVar = std::getenv("XCL_EMULATION_MODE") ;

    if (!envVar)                                 mode = HW ;
    else if (std::strcmp(envVar, "sw_emu") == 0) mode = SW_EMU ;
    else if (std::strcmp(envVar, "hw_emu") == 0) mode = HW_EMU ;

    return mode ;
  }

} // end namespace xdp
