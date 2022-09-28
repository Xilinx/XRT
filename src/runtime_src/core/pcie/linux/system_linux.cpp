// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#include "system_linux.h"
#include "device_linux.h"
#include "core/common/query_requests.h"
#include "gen/version.h"
#include "pcidev.h"
#include "pcidrv_xocl.h"
#include "pcidrv_xclmgmt.h"

#include <boost/property_tree/ini_parser.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

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

xrt_core::system_linux* singleton = nullptr;

// Singleton registers with base class xrt_core::system
// during static global initialization.  If statically
// linking with libxrt_core, then explicit initialiation
// is required
inline xrt_core::system_linux&
singleton_instance()
{
  if (!singleton)
    static xrt_core::system_linux s;

  if (singleton)
    return *singleton;

  throw std::runtime_error("system_linux singleton is not initialized");
}

// Dynamic linking automatically constructs the singleton
// Do not instantiate singleton, if SHIM is extended outside.
#ifndef EXTERNAL_SHIM
struct X
{
  X() { singleton_instance(); }
} x;
#endif

boost::property_tree::ptree
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

}

namespace xrt_core {

void
system_linux::
register_driver(std::shared_ptr<pcidrv::pci_driver> driver)
{
  driver_list.push_back(driver);

  namespace bfs = boost::filesystem;
  const std::string drv_root = "/sys/bus/pci/drivers/";
  const std::string drvpath = drv_root + driver->name();

  if(!bfs::exists(drvpath))
    return;

  // Gather all sysfs directory and sort
  std::vector<bfs::path> vec{bfs::directory_iterator(drvpath), bfs::directory_iterator()};
  std::sort(vec.begin(), vec.end());

  for (auto& path : vec) {
    try {
      auto pf = driver->create_pcidev(path.filename().string());

      // In docker, all host sysfs nodes are available. So, we need to check
      // devnode to make sure the device is really assigned to docker.
      if (!bfs::exists(pf->get_subdev_path("", -1)))
        continue;

      // Insert detected device into proper list.
      if (pf->is_mgmt) {
        if (pf->is_ready)
          mgmt_ready_list.push_back(pf);
        else
          mgmt_nonready_list.push_back(pf);
      } else {
        if (pf->is_ready)
          user_ready_list.push_back(pf);
        else
          user_nonready_list.push_back(pf);
      }
    } catch (std::invalid_argument const& ex) {
      continue;
    }
  }
}

std::shared_ptr<pcidev::pci_device>
system_linux::
get_pcidev(unsigned index, bool is_user) const
{
  if (is_user) {
    if (index < user_ready_list.size())
      return user_ready_list[index];
    return user_nonready_list[index - user_ready_list.size()];
  }

  if (index < mgmt_ready_list.size())
    return mgmt_ready_list[index];
  return mgmt_nonready_list[index - mgmt_ready_list.size()];
}

size_t
system_linux::
get_num_dev_ready(bool is_user) const
{
  if (is_user)
    return user_ready_list.size();
  return mgmt_ready_list.size();
}

size_t
system_linux::
get_num_dev_total(bool is_user) const
{
  if (is_user)
    return user_ready_list.size() + user_nonready_list.size();
  return mgmt_ready_list.size() + mgmt_nonready_list.size();
}

system_linux::
system_linux()
{
  if (singleton)
    throw std::runtime_error("More than one SHIM registered!");
  singleton = this;

  register_driver(std::make_shared<pcidrv::pci_driver_xocl>());
  register_driver(std::make_shared<pcidrv::pci_driver_xclmgmt>());
}

void
system_linux::
get_xrt_info(boost::property_tree::ptree &pt)
{
  boost::property_tree::ptree _ptDriverInfo;

  for (auto drv : driver_list)
    _ptDriverInfo.push_back(std::make_pair("", driver_version(drv->name())));
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

  char hostname[256] = {0};
  gethostname(hostname, 256);
  std::string hn(hostname);
  pt.put("hostname", hn);
}

device::id_type
system_linux::
get_device_id(const std::string& bdf) const
{
  // Treat non bdf as device index
  if (bdf.find_first_not_of("0123456789") == std::string::npos)
    return system::get_device_id(bdf);
    
  unsigned int i = 0;
  for (auto dev = get_pcidev(i); dev; i++, dev = get_pcidev(i)) {
      // [dddd:bb:dd.f]
      auto dev_bdf = boost::str(boost::format("%04x:%02x:%02x.%01x") % dev->domain % dev->bus % dev->dev % dev->func);
      if (dev_bdf == bdf)
        return i;
      //consider default domain as 0000 and try to find a matching device
      if(dev->domain == 0) {
        dev_bdf = boost::str(boost::format("%02x:%02x.%01x") % dev->bus % dev->dev % dev->func);
        if(dev_bdf == bdf)
          return i;
      }
  }

  throw xrt_core::system_error(EINVAL, "No such device '" + bdf + "'");
}

std::pair<device::id_type, device::id_type>
system_linux::
get_total_devices(bool is_user) const
{
  return std::make_pair(pcidev::get_dev_total(is_user), pcidev::get_dev_ready(is_user));
}

std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
system_linux::
get_bdf_info(device::id_type id, bool is_user) const
{
  auto pdev = get_pcidev(id, is_user);
  return std::make_tuple(pdev->domain, pdev->bus, pdev->dev, pdev->func);
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
  auto pdev = get_pcidev(id, true);
  return pdev->create_device(handle, id);
}

std::shared_ptr<device>
system_linux::
get_mgmtpf_device(device::id_type id) const
{
  auto pdev = get_pcidev(id, false);
  return pdev->create_device(nullptr, id);
}

void
system_linux::
program_plp(const device* dev, const std::vector<char> &buffer, bool force) const
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

namespace pcidev {

size_t
get_dev_ready(bool user)
{
  return singleton_instance().get_num_dev_ready(user);
}

size_t
get_dev_total(bool user)
{
  return singleton_instance().get_num_dev_total(user);
}

std::shared_ptr<pci_device>
get_dev(unsigned index, bool user)
{
  return singleton_instance().get_pcidev(index, user);
}

} // pcidev
