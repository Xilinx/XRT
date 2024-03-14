/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_CORE_SOURCE

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <ctime>
#include <cstring>
#include <fstream>
#include <sstream>

#include "core/common/sysinfo.h"

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

  std::string getMsecSinceEpoch() 
  {
    auto timeSinceEpoch = (std::chrono::system_clock::now()).time_since_epoch();
    auto value = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceEpoch);
    return std::to_string(value.count());
  }

  const char* getToolVersion()
  {
    return "2024.1";
  }

  std::string getXRTVersion()
  {
    static std::string version = "";
    if (version != "")
      return version;

    boost::property_tree::ptree xrtInfo;
    xrt_core::sysinfo::get_xrt_info(xrtInfo);

    version = xrtInfo.get<std::string>("version", "N/A");

    return version;
  }

  // This function can only be called after the system singleton has
  //  been created on the XRT side.  This means we cannot call it in any
  //  plugin constructor.
  bool isEdge()
  {
    static bool initialized = false;
    static bool storedValue = false;

    if (initialized)
      return storedValue;

    initialized = true;

    boost::property_tree::ptree pt;
    xrt_core::sysinfo::get_xrt_info(pt);

    try {
      for (boost::property_tree::ptree::value_type& info : pt.get_child("drivers")) {
        try {
          std::string str = info.second.get<std::string>("name");
          if(0 == str.compare("zocl")) {
            storedValue = true;
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

  // Get the size of the physical device memory (in bytes) when running
  // on the PS of Edge boards.  If called on x86 or Windows this should
  // return 0.
  uint64_t getPSMemorySize()
  {
#ifndef _WIN32
    if (!isEdge())
      return 0;

    try {
      std::string line;
      std::ifstream ifs;
      ifs.open("/proc/meminfo");
      while (getline(ifs, line)) {
        if (line.find("CmaTotal") == std::string::npos)
          continue;

        // Memory sizes are always expressed in kB
        std::vector<std::string> cmaVector;
        boost::split(cmaVector, line, boost::is_any_of(":"));
        return std::stoull(cmaVector.at(1)) * 1024;
      }
    }
    catch (...) {
      // Do nothing
    }
#endif
    return 0;
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
