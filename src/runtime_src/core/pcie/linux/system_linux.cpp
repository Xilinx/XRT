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

#include "system_linux.h"
#include "device_linux.h"
#include "gen/version.h"
#include "scan.h"
#include "core/common/time.h"

#include <boost/property_tree/ini_parser.hpp>

#include <fstream>
#include <memory>

#include <sys/utsname.h>
#include <gnu/libc-version.h>

namespace {

static std::string
driver_version(const std::string& driver)
{
  std::string line("unknown");
  std::string path("/sys/module/");
  path += driver;
  path += "/version";
  std::ifstream ver(path);
  if (ver.is_open()) {
    getline(ver, line);
  }

  return line;
}

static std::vector<std::weak_ptr<xrt_core::device_linux>> mgmtpf_devices(16); // fix size
static std::vector<std::weak_ptr<xrt_core::device_linux>> userpf_devices(16); // fix size

}

namespace xrt_core {

system*  
system_child_ctor()
{
  static system_linux sl;
  return &sl;
}

void 
system_linux::
get_xrt_info(boost::property_tree::ptree &pt)
{
  pt.put("version",   xrt_build_version);
  pt.put("hash",      xrt_build_version_hash);
  pt.put("date",      xrt_build_version_date);
  pt.put("branch",    xrt_build_version_branch);
  pt.put("xocl",      driver_version("xocl"));
  pt.put("xclmgmt",   driver_version("xclmgmt"));
}


void 
system_linux::
get_os_info(boost::property_tree::ptree &pt)
{
  struct utsname sysinfo;
  if (!uname(&sysinfo)) {
    pt.put("sysname",   sysinfo.sysname);
    pt.put("release",   sysinfo.release);
    pt.put("version",   sysinfo.version);
    pt.put("machine",   sysinfo.machine);
  }

  pt.put("glibc", gnu_get_libc_version());

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
          pt.put("linux", val);
      }
      ifs.close();
  }

  pt.put("now", xrt_core::timestamp());
}

std::pair<device::id_type, device::id_type>
system_linux::
get_total_devices() const
{
  return std::make_pair(pcidev::get_dev_total(), pcidev::get_dev_ready());
}

void
system_linux::
scan_devices(bool verbose, bool json) const
{
  std::cout << "TO-DO: scan_devices\n";
  verbose = verbose;
  json = json;
}

std::shared_ptr<device>
system_linux::
get_userpf_device(device::id_type id) const
{
  // check cache
  auto device = userpf_devices[id].lock();
  if (!device) {
    device = std::shared_ptr<device_linux>(new device_linux(id,true));
    userpf_devices[id] = device;
  }
  return device;
}

std::shared_ptr<device>
system_linux::
get_mgmtpf_device(device::id_type id) const
{
  // check cache
  auto device = mgmtpf_devices[id].lock();
  if (!device) {
    device = std::shared_ptr<device_linux>(new device_linux(id,false));
    mgmtpf_devices[id] = device;
  }
  return device;
}

} // xrt_core
