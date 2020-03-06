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

#include "xdp/profile/plugin/vp_base/utility.h"

#ifdef _WIN32
#pragma warning(disable : 4996)
/* Disable warning for use of "localtime" */
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
    return "2019.2" ;
  }

  const char* getXRTVersion()
  {
    return "2.6.0" ;	// To Do
  }

} // end namespace xdp
