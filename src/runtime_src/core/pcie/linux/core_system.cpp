/**
 * Copyright (C) 2019 Xilinx, Inc
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


#include "common/core_system.h"
#include "gen/version.h"

#include <sys/utsname.h>
#include <gnu/libc-version.h>

#include <boost/property_tree/ini_parser.hpp>

#include <string>
#include <fstream>

#include <chrono>

#include "scan.h"

static std::string driver_version(const std::string & _driver)
{
  std::string line("unknown");
  std::string path("/sys/module/");
  path += _driver;
  path += "/version";
  std::ifstream ver(path);
  if (ver.is_open()) {
    getline(ver, line);
  }

  return line;
}

void 
xrt_core::system::get_xrt_info(boost::property_tree::ptree &_pt)
{
  _pt.put("version",   xrt_build_version);
  _pt.put("hash",      xrt_build_version_hash);
  _pt.put("date",      xrt_build_version_date);
  _pt.put("branch",    xrt_build_version_branch);
  _pt.put("xocl",      driver_version("xocl"));
  _pt.put("xclmgmt",   driver_version("xclmgmt"));
}


void 
xrt_core::system::get_os_info(boost::property_tree::ptree &_pt)
{
  struct utsname sysinfo;
  if (!uname(&sysinfo)) {
    _pt.put("sysname",   sysinfo.sysname);
    _pt.put("release",   sysinfo.release);
    _pt.put("version",   sysinfo.version);
    _pt.put("machine",   sysinfo.machine);
  }

  _pt.put("glibc", gnu_get_libc_version());

  // The file is a requirement as per latest Linux standards
  // https://www.freedesktop.org/software/systemd/man/os-release.html
  std::ifstream ifs("/etc/os-release");
  if ( ifs.good() ) {
      boost::property_tree::ptree opt;
      boost::property_tree::ini_parser::read_ini(ifs, opt);
      std::string val = opt.get<std::string>("PRETTY_NAME", "");
      if (val.length()) {
          if ((val.front() == '"') && (val.back() == '"')) {
              val.erase(0, 1);
              val.erase(val.size()-1);
          }
          _pt.put("linux", val);
      }
      ifs.close();
  }

  // Cannot use xrt_core::timestamp() defined in common/t_time because
  // it adds [] around the string
  auto n = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(n);
  std::string tnow(std::ctime(&t));
  // Strip out the newline at the end
  tnow.pop_back();
  _pt.put("now", tnow);
}




