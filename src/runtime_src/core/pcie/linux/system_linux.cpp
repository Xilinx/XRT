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
#include "core/common/query_requests.h"
#include "gen/version.h"
#include "scan.h"
#include "core/pcie/common/memaccess.h"

#include <boost/property_tree/ini_parser.hpp>
#include <boost/format.hpp>

#include <fstream>
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <thread>

#include <sys/utsname.h>
#include <gnu/libc-version.h>
#include <unistd.h>

#if defined(__aarch64__) || defined(__arm__) || defined(__mips__)
  #define MACHINE_NODE_PATH "/proc/device-tree/model"
#elif defined(__PPC64__)
  #define MACHINE_NODE_PATH "/proc/device-tree/model-name"
  // /proc/device-tree/system-id may be 000000
  // /proc/device-tree/model may be 00000
#elif defined (__x86_64__)
  #define MACHINE_NODE_PATH "/sys/devices/virtual/dmi/id/product_name"
#else
#error "Unsupported platform"
  #define MACHINE_NODE_PATH ""
#endif

namespace {

// Singleton registers with base class xrt_core::system
// during static global initialization.  If statically
// linking with libxrt_core, then explicit initialiation
// is required
static xrt_core::system_linux*
singleton_instance()
{
  static xrt_core::system_linux singleton;
  return &singleton;
}

// Dynamic linking automatically constructs the singleton
struct X
{
  X() { singleton_instance(); }
} x;

static boost::property_tree::ptree
driver_version(const std::string& driver)
{
  boost::property_tree::ptree _pt;
  std::string ver("unknown");
  std::string hash("unknown");
  std::string path("/sys/module/");
  path += driver;
  path += "/version";

  std::ifstream stream(path);
  if (stream.is_open()) {
    std::string line;
    getline(stream, line);
    std::stringstream ss(line);
    getline(ss, ver, ',');
    getline(ss, hash, ',');
  }

  _pt.put("name", driver);
  _pt.put("version", ver);
  _pt.put("hash", hash);
  return _pt;
}

static boost::property_tree::ptree
glibc_info()
{
  boost::property_tree::ptree _pt;
  _pt.put("name", "glibc");
  _pt.put("version", gnu_get_libc_version());
  return _pt;
}

static std::string machine_info()
{
  std::string model("unknown");
  std::ifstream stream(MACHINE_NODE_PATH);
  if (stream.good()) {
    std::getline(stream, model);
    stream.close();
  }
  return model;
}

static std::vector<std::weak_ptr<xrt_core::device_linux>> mgmtpf_devices(16); // fix size
static std::vector<std::weak_ptr<xrt_core::device_linux>> userpf_devices(16); // fix size
static std::map<xrt_core::device::handle_type, std::weak_ptr<xrt_core::device_linux>> userpf_device_map;

}

namespace xrt_core {


void
system_linux::
get_xrt_info(boost::property_tree::ptree &pt)
{
  boost::property_tree::ptree _ptDriverInfo;
  _ptDriverInfo.push_back( std::make_pair("", driver_version("xocl") ));
  _ptDriverInfo.push_back( std::make_pair("", driver_version("xclmgmt") ));
  pt.put_child("drivers", _ptDriverInfo);
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
          pt.put("distribution", val);
      }
      ifs.close();
  }

  pt.put("model", machine_info());
  pt.put("cores", std::thread::hardware_concurrency());
  pt.put("memory_bytes", (boost::format("0x%lx") % (sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE))).str());
  boost::property_tree::ptree _ptLibInfo;
  _ptLibInfo.push_back( std::make_pair("", glibc_info() ));
  pt.put_child("libraries", _ptLibInfo);
}

device::id_type
system_linux::
get_device_id(const std::string& bdf) const
{
  // Treat non bdf as device index
  if (bdf.find_first_not_of("0123456789") == std::string::npos)
    return system::get_device_id(bdf);
    
  unsigned int i = 0;
  for (auto dev = pcidev::get_dev(i); dev; i++, dev = pcidev::get_dev(i)) {
      // [dddd:bb:dd.f]
      auto dev_bdf = boost::str(boost::format("%04x:%02x:%02x.%01x") % dev->domain % dev->bus % dev->dev % dev->func);
      if (dev_bdf == bdf)
        return i;
  }

  throw xrt_core::system_error(EINVAL, "No such device '" + bdf + "'");
}

std::pair<device::id_type, device::id_type>
system_linux::
get_total_devices(bool is_user) const
{
  return std::make_pair(pcidev::get_dev_total(is_user), pcidev::get_dev_ready(is_user));
}

void
system_linux::
scan_devices(bool, bool) const
{
  std::cout << "TO-DO: scan_devices\n";
}

std::shared_ptr<device>
system_linux::
get_userpf_device(device::id_type id) const
{
  return xrt_core::get_userpf_device(xclOpen(id, nullptr, XCL_QUIET));
}

std::shared_ptr<device>
system_linux::
get_userpf_device(device::handle_type handle, device::id_type id) const
{
  // deliberately not using std::make_shared (used with weak_ptr)
  return std::shared_ptr<device_linux>(new device_linux(handle, id, true));
}

std::shared_ptr<device>
system_linux::
get_mgmtpf_device(device::id_type id) const
{
  // deliberately not using std::make_shared (used with weak_ptr)
  return std::shared_ptr<device_linux>(new device_linux(nullptr, id, false));
}

void
system_linux::
program_plp(const device* dev, const std::vector<char> &buffer) const
{
  try {
    xrt_core::scope_value_guard<int, std::function<void()>> fd = dev->file_open("icap", O_WRONLY);
    auto ret = write(fd.get(), buffer.data(), buffer.size());
    if (static_cast<size_t>(ret) != buffer.size())
      throw xrt_core::error(EINVAL, "Write plp to icap subdev failed");

  } catch (const std::exception& e) {
    xrt_core::send_exception_message(e.what(), "XBMGMT");
  }

  auto value = xrt_core::query::rp_program_status::value_type(1);
  xrt_core::device_update<xrt_core::query::rp_program_status>(dev, value);

  // asynchronously check if the download is complete
  const static int program_timeout_sec = 60;
  bool is_complete = false;
  int retry_count = 0;
  while (!is_complete && retry_count++ < program_timeout_sec) {
    is_complete = xrt_core::query::rp_program_status::to_bool(xrt_core::device_query<xrt_core::query::rp_program_status>(dev));
    if (retry_count == program_timeout_sec)
      throw xrt_core::error(ETIMEDOUT, "PLP programmming timed out");

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void
system_linux::
mem_read(const device* device, long long addr, long long size, const std::string& output_file) const
{
  auto get_ddr_mem_size = [device]() {
    auto ddr_size = xrt_core::device_query<xrt_core::query::rom_ddr_bank_size_gb>(device);
    auto ddr_bank_count = xrt_core::device_query<xrt_core::query::rom_ddr_bank_count_max>(device);

    // convert ddr_size from GB to bytes
    // return the result in KB
    return (ddr_size << 30) * ddr_bank_count / (1024 * 1024);
  };
  auto bdf_str = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
  auto handle = device->get_device_handle();
  if(xcldev::memaccess(handle, get_ddr_mem_size(), getpagesize(), bdf_str)
      .read(output_file, addr, size) < 0)
    throw xrt_core::error(EINVAL, "Memory read failed");
}

void
system_linux::
mem_write(const device* device, long long addr, long long size, unsigned int pattern) const
{
  auto get_ddr_mem_size = [device]() {
    auto ddr_size = xrt_core::device_query<xrt_core::query::rom_ddr_bank_size_gb>(device);
    auto ddr_bank_count = xrt_core::device_query<xrt_core::query::rom_ddr_bank_count_max>(device);
    
    // convert ddr_size from GB to bytes
    // return the result in KB
    return (ddr_size << 30) * ddr_bank_count / (1024 * 1024);
  };

  auto bdf_str = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
  auto handle = device->get_device_handle();
  if(xcldev::memaccess(handle, get_ddr_mem_size(), getpagesize(), bdf_str)
      .write(addr, size, pattern) < 0)
    throw xrt_core::error(EINVAL, "Memory write failed");
}

namespace pcie_linux {

std::shared_ptr<device>
get_userpf_device(device::handle_type device_handle, device::id_type id)
{
  singleton_instance(); // force loading if necessary
  return xrt_core::get_userpf_device(device_handle, id);
}

device::id_type
get_device_id_from_bdf(const std::string& bdf)
{
  singleton_instance(); // force loading if necessary
  return xrt_core::get_device_id(bdf);
}

} // pcie_linux

} // xrt_core
