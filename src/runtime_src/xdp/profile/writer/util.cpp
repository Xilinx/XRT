/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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

#include "util.h"
#include <chrono>
#include <ctime>
#include "core/common/core_system.h"

#if defined(__linux__) && defined(__x86_64__)
#include <unistd.h>
#endif

#ifdef _WIN32
#pragma warning(disable : 4996)
/* Disable warning for use of "localtime" */
#endif

namespace xdp {

  std::string WriterI::getXRTVersion()
  {
    std::string xrtVersion;

    boost::property_tree::ptree xrtInfo;
    xrt_core::system::get_xrt_info(xrtInfo);
    xrtVersion = "XRT build version: "  + xrtInfo.get<std::string>("version", "N/A") + "\n"
               + "Build version branch: " + xrtInfo.get<std::string>("branch", "N/A") + "\n"
               + "Build version hash: " + xrtInfo.get<std::string>("hash", "N/A") + "\n"
               + "Build version date: " + xrtInfo.get<std::string>("date", "N/A") + " ";

    return xrtVersion;
  }

  std::string WriterI::getCurrentDateTime()
  {
    auto time = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    //It seems std::put_time is not yet available
    //std::stringstream ss;
    //ss << std::put_time(std::localtime(&time), "%Y-%m-%d %X");
    //return ss.str();

    struct tm *p_tstruct = std::localtime(&time);
    if(p_tstruct) {
        char buf[80] = {0};
        strftime(buf, sizeof(buf), "%Y-%m-%d %X", p_tstruct);
        return std::string(buf);
    }
    return std::string("0000-00-00 0000");
  }

  std::string WriterI::getCurrentTimeMsec()
  {
    auto timeSinceEpoch = (std::chrono::system_clock::now()).time_since_epoch();
    auto value = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceEpoch);
    uint64_t timeMsec = value.count();
    return std::to_string(timeMsec);

#if 0
    struct timespec now;
    int err;
    if ((err = clock_gettime(CLOCK_REALTIME, &now)) < 0)
      return "0";

    uint64_t nsec = (uint64_t) now.tv_sec * 1000000000UL + (uint64_t) now.tv_nsec;
    uint64_t msec = nsec / 1e6;
    return std::to_string(msec);
#endif
  }

  std::string WriterI::getCurrentExecutableName()
  {
    std::string execName("");
#if defined(__linux__) && defined(__x86_64__)
    const int maxLength = 1024;
    char buf[maxLength];
    ssize_t len;
    if ((len=readlink("/proc/self/exe", buf, maxLength-1)) != -1) {
      buf[len]= '\0';
      execName = buf;
    }

    // Remove directory path and only keep filename
    const size_t lastSlash = execName.find_last_of("\\/");
    if (lastSlash != std::string::npos)
      execName.erase(0, lastSlash + 1);
#endif
    return execName;
  }

} // xdp
