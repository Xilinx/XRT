// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
#include "system.h"
#include "device.h"
#include "module_loader.h"
#include "gen/version.h"
#include "query_requests.h"

#include <boost/format.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <thread>
#include <vector>

#ifdef __linux__
#include <gnu/libc-version.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <setupapi.h>
#include <windows.h>
#pragma warning (disable : 4996)
#pragma comment (lib, "Setupapi.lib")
#endif

#if defined(__aarch64__) || defined(__arm__) || defined(__mips__)
#define MACHINE_NODE_PATH "/proc/device-tree/model"
#define PFM_NAME "edge"
#elif defined(__PPC64__)
#define MACHINE_NODE_PATH "/proc/device-tree/model-name"
#define PFM_NAME "edge"
// /proc/device-tree/system-id may be 000000
// /proc/device-tree/model may be 00000
#elif defined (__x86_64__)
#define MACHINE_NODE_PATH "/sys/devices/virtual/dmi/id/product_name"
#define PFM_NAME "pcie"
#elif defined (_WIN32)
#define MACHINE_NODE_PATH ""
#define PFM_NAME "pcie"
#else
#error "Unsupported platform"
#define MACHINE_NODE_PATH ""
#define PFM_NAME ""
#endif

namespace {

static std::map<xrt_core::device::id_type, std::weak_ptr<xrt_core::device>> mgmtpf_device_map;
static std::map<xrt_core::device::handle_type, std::weak_ptr<xrt_core::device>> userpf_device_map;
static const char* pcie_pfm = "pcie";
//static const char* edge_pfm = "edge";
// mutex to protect insertion
static std::mutex mutex;

#ifdef __linux__
static boost::property_tree::ptree
driver_version(const std::string& driver)
{
  boost::property_tree::ptree _pt;
  std::string ver("unknown");
  std::string hash("unknown");

  if (std::strcmp(PFM_NAME, pcie_pfm) == 0) {
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
  }
  else {
    //dkms flow is not available for zocl
   //so version.h file is not available at zocl build time
#if defined(XRT_DRIVER_VERSION)
    std::string zocl_driver_ver = XRT_DRIVER_VERSION;
    std::stringstream ss(zocl_driver_ver);
    getline(ss, ver, ',');
    getline(ss, hash, ',');
#endif
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

static std::string
machine_info()
{
  std::string model("unknown");
  std::ifstream stream(MACHINE_NODE_PATH);
  if (stream.good()) {
    std::getline(stream, model);
    stream.close();
  }
  return model;
}
#endif

#ifdef _WIN32
static std::string
getmachinename()
{
  std::string machine;
  SYSTEM_INFO sysInfo;

  // Get hardware info
  ZeroMemory(&sysInfo, sizeof(SYSTEM_INFO));
  GetSystemInfo(&sysInfo);
  // Set processor architecture
  switch (sysInfo.wProcessorArchitecture) {
  case PROCESSOR_ARCHITECTURE_AMD64:
    machine = "x86_64";
    break;
  case PROCESSOR_ARCHITECTURE_IA64:
    machine = "ia64";
    break;
  case PROCESSOR_ARCHITECTURE_INTEL:
    machine = "x86";
    break;
  case PROCESSOR_ARCHITECTURE_UNKNOWN:
  default:
    machine = "unknown";
    break;
  }
  return machine;
}

static std::string
osNameImpl()
{
  OSVERSIONINFO vi;
  vi.dwOSVersionInfoSize = sizeof(vi);
  if (GetVersionEx(&vi) == 0)
    throw xrt_core::error("Cannot get OS version information");
  switch (vi.dwPlatformId)
  {
  case VER_PLATFORM_WIN32s:
    return "Windows 3.x";
  case VER_PLATFORM_WIN32_WINDOWS:
    return vi.dwMinorVersion == 0 ? "Windows 95" : "Windows 98";
  case VER_PLATFORM_WIN32_NT:
    return "Windows NT";
  default:
    return "Unknown";
  }
}
#endif

}

namespace xrt_core {

static std::vector<std::shared_ptr<xrt_core::device_factory>> user_ready_list;
static std::vector<std::shared_ptr<xrt_core::device_factory>> user_nonready_list;
static std::vector<std::shared_ptr<xrt_core::device_factory>> mgmt_ready_list;
static std::vector<std::shared_ptr<xrt_core::device_factory>> mgmt_nonready_list;  

// Singleton is initialized when libxrt_core is loaded
// A concrete system object is constructed during static
// global initialization.  Lifetime is until core library
// is unloaded.
system* singleton = nullptr;

system::
system()
{
  try {
    static xrt_core::shim_loader shim;//xrt_core
    static xrt_core::driver_loader plugins;
  }
  catch (const std::runtime_error& err) {
    xrt_core::send_exception_message(err.what(), "WARNING");
  }

  std::cout << "system :: size of user_ready_list : " << user_ready_list.size() << std::endl;
  std::cout << "system :: size of user_nonready_list : " << user_nonready_list.size() << std::endl;
  std::cout << "system :: size of mgmt_ready_list : " << mgmt_ready_list.size() << std::endl;
  std::cout << "system :: size of mgmt_nonready_list : " << mgmt_nonready_list.size() << std::endl;
}

void
system::
get_devices(boost::property_tree::ptree& pt) const
{
  auto cards = get_total_devices();
  using index_type = decltype(cards.first);

  boost::property_tree::ptree pt_devices;
  for (index_type device_id = 0; device_id < cards.first; ++device_id) {
    boost::property_tree::ptree pt_device;

    // Key: device_id
    pt_device.put("device_id", std::to_string(device_id));

    // Key: pcie
    auto device = xrt_core::get_userpf_device(device_id);
    boost::property_tree::ptree pt_pcie;
    device->get_info(pt_pcie);
    pt_device.add_child(PFM_NAME, pt_pcie);

    // Create our array of data
    pt_devices.push_back(std::make_pair("", pt_device));
  }

  pt.add_child("devices", pt_devices);
}

std::shared_ptr<device_factory>
system::
get_device(unsigned index, bool is_user) const
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
system::
get_num_dev_ready(bool is_user) const
{
  if (is_user)
    return user_ready_list.size();

  return mgmt_ready_list.size();
}

size_t
system::
get_num_dev_total(bool is_user) const
{
  if (is_user)
    return user_ready_list.size() + user_nonready_list.size();

  return mgmt_ready_list.size() + mgmt_nonready_list.size();
}


// get_device_id() -  Default conversion of device string
// Redefined in systems that support BDF.
device::id_type
system::
get_device_id_default(const std::string& str) const
{
  size_t pos = 0;
  auto id = std::stoul(str, &pos);
  if (pos != str.length())
    throw xrt_core::system_error(EINVAL, "Invalid device string '" + str + "'");
  return static_cast<device::id_type>(id);
}

void
system::
get_xrt_info(boost::property_tree::ptree& pt)
{
  boost::property_tree::ptree _ptDriverInfo;
#ifdef __linux__
  if (std::strcmp(PFM_NAME, pcie_pfm) == 0) { //pcie
    //for (const auto& drv : xrt_core::pci::get_driver_list()) // TODO::?
    //  _ptDriverInfo.push_back( {"", driver_version(drv->name())} ); 
    _ptDriverInfo.push_back({ "", driver_version("xocl") });
    _ptDriverInfo.push_back({ "", driver_version("xclmgmt") });
  }
  else { //edge
    pt.put("build.version", xrt_build_version);
    pt.put("build.hash", xrt_build_version_hash);
    pt.put("build.date", xrt_build_version_date);
    pt.put("build.branch", xrt_build_version_branch);
    _ptDriverInfo.push_back(std::make_pair("", driver_version("zocl")));
  }
#endif
  pt.put_child("drivers", _ptDriverInfo);
}

void
system::
get_os_info(boost::property_tree::ptree& pt)
{
#ifdef __linux__
  struct utsname sysinfo;
  if (!uname(&sysinfo)) {
    pt.put("sysname", sysinfo.sysname);
    pt.put("release", sysinfo.release);
    pt.put("version", sysinfo.version);
    pt.put("machine", sysinfo.machine);
  }

  // The file is a requirement as per latest Linux standards
  std::ifstream ifs("/etc/os-release");
  if (ifs.good()) {
    boost::property_tree::ptree opt;
    boost::property_tree::ini_parser::read_ini(ifs, opt);
    std::string val = opt.get<std::string>("PRETTY_NAME", "");
    if (val.length()) {
      if ((val.front() == '"') && (val.back() == '"')) {
        val.erase(0, 1);
        val.erase(val.size() - 1);
      }
      pt.put("distribution", val);
    }
    ifs.close();
  }

  pt.put("model", machine_info());
  pt.put("cores", std::thread::hardware_concurrency());
  pt.put("memory_bytes", (boost::format("0x%lx") % (sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE))).str());
  //pt.put("now", xrt_core::timestamp());
  boost::property_tree::ptree _ptLibInfo;
  _ptLibInfo.push_back({ "", glibc_info() });
  pt.put_child("libraries", _ptLibInfo);

#ifndef XRT_EDGE
  char hostname[256] = { 0 };
  gethostname(hostname, 256);
  std::string hn(hostname);
  pt.put("hostname", hn);
#endif

#endif

#ifdef _WIN32
  char value[128];
  DWORD BufferSize = sizeof value;
  pt.put("sysname", osNameImpl());
  //Reassign buffer size since it get override with size of value by RegGetValueA() call
  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "BuildLab", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("release", value);
  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentVersion", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("version", value);
  pt.put("machine", getmachinename());

  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("distribution", value);

  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\SystemInformation", "SystemProductName", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("model", value);

  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ComputerName", "ComputerName", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("hostname", value);

  MEMORYSTATUSEX mem;
  mem.dwLength = sizeof(mem);
  GlobalMemoryStatusEx(&mem);
  pt.put("memory_bytes", (boost::format("0x%llx") % mem.ullTotalPhys).str());
  pt.put("cores", std::thread::hardware_concurrency());
#endif
}

device::id_type
system::
get_device_id(const std::string& bdf) const
{
  // Treat non bdf as device index
  if (bdf.find_first_not_of("0123456789") == std::string::npos)
    return system::get_device_id_default(bdf);

  unsigned int i = 0;
  for (auto dev = get_device(0); dev; dev = get_device(++i)) {
    // [dddd:bb:dd.f]
    auto bdf_tuple = dev->get_bdf_info();
    auto dev_bdf = boost::str(boost::format("%04x:%02x:%02x.%01x") % std::get<0>(bdf_tuple) % std::get<1>(bdf_tuple) % std::get<2>(bdf_tuple) % std::get<3>(bdf_tuple));
    if (dev_bdf == bdf)
      return i;
    //consider default domain as 0000 and try to find a matching device
    if (std::get<0>(bdf_tuple) == 0) {
      dev_bdf = boost::str(boost::format("%02x:%02x.%01x") % std::get<1>(bdf_tuple) % std::get<2>(bdf_tuple) % std::get<3>(bdf_tuple));
      if (dev_bdf == bdf)
        return i;
    }
  }

  throw xrt_core::system_error(EINVAL, "No such device '" + bdf + "'");
}

std::pair<device::id_type, device::id_type>
system::
get_total_devices(bool is_user) const
{
  return std::make_pair(static_cast<device::id_type>(get_num_dev_total(is_user)), static_cast<device::id_type>(get_num_dev_ready(is_user)));
}

std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
system::
get_bdf_info(device::id_type id, bool is_user) const
{
  auto pdev = get_device(id, is_user);
  if(!pdev)
    return std::make_tuple(uint16_t(0), uint16_t(0), uint16_t(0), uint16_t(0));
  return pdev->get_bdf_info();
}

std::shared_ptr<device>
system::
get_userpf_device(device::id_type id) const
{
  auto pdev = get_device(id, true);
  if (!pdev)
    return nullptr;
  return xrt_core::get_userpf_device(pdev->create_shim(id));
}

std::shared_ptr<device>
system::
get_userpf_device(device::handle_type handle, device::id_type id) const
{
  auto pdev = get_device(id, true);
  return pdev->create_device(handle, id);
}

std::shared_ptr<device>
system::
get_mgmtpf_device(device::id_type id) const
{
  auto pdev = get_device(id, false);
  return pdev->create_device(nullptr, id);
}

system::monitor_access_type
system::
get_monitor_access_type() const
{  
  return (std::strcmp(PFM_NAME, pcie_pfm) == 0)? monitor_access_type::ioctl : monitor_access_type::bar;
}

void
system::
program_plp(const device* dev, const std::vector<char>& buffer, bool force) const
{
  if (!dev)
    throw xrt_core::error("system program_plp - Invalid device");
  dev->program_plp(buffer, force);
}


//static void
//load_shim()
//{
//  // This is where the xrt_core library is loaded at run-time. Loading
//  // of the library will create an instance of the system singleton
//  // and set the singleton variable in this file. However, the
//  // singleton, while set, can not be assumed to be valid until after
//  // this function returns.  This is because the derived system class
//  // could have constructor body that is executed after the base
//  // class is constructed.
//  
//  try {
//    static xrt_core::shim_loader shim;
//    static xrt_core::driver_loader plugins;
//  }
//  catch (const std::runtime_error& err) {
//    xrt_core::send_exception_message(err.what(), "WARNING");
//  }
//}

inline system&
instance()
{
  // Multiple threads could enter here at the same time.  The first
  // thread will call the shim loader, where the singleton is set, but
  // not necessarily ready.  See comment in load_shim().
  static std::mutex mtx;
  std::lock_guard lk(mtx);

  if (!singleton) {    
    singleton = new xrt_core::system();
  }

  if (singleton)
    return *singleton;

  throw std::runtime_error("system singleton is not loaded");
}

void
get_xrt_build_info(boost::property_tree::ptree& pt)
{
  pt.put("version",    xrt_build_version);
  pt.put("branch",     xrt_build_version_branch);
  pt.put("hash",       xrt_build_version_hash);
  pt.put("build_date", xrt_build_version_date);
}

void
get_xrt_info(boost::property_tree::ptree &pt)
{
  get_xrt_build_info(pt);
  instance().get_xrt_info(pt);
}

void
get_os_info(boost::property_tree::ptree& pt)
{
  instance().get_os_info(pt);
}

void
get_devices(boost::property_tree::ptree& pt)
{
  instance().get_devices(pt);
}

std::shared_ptr<device>
get_userpf_device(device::id_type id)
{
  // Construct device by calling xclOpen, the returned
  // device is cached and unmanaged
  auto device = instance().get_userpf_device(id);

  if (!device)
    throw std::runtime_error("Could not open device with index '"+ std::to_string(id) + "'");

  // Repackage raw ptr in new shared ptr with deleter that calls xclClose,
  // but leaves device object alone. The returned device is managed in that
  // it calls xclClose when going out of scope.
  auto close = [] (xrt_core::device* d) { d->close_device(); };
  return {device.get(), close};
}

std::shared_ptr<device>
get_userpf_device(device::handle_type handle)
{
  // Look up core device from low level shim handle The handle is
  // inserted into map as part of calling xclOpen.  Protect against
  // multiple threads calling xclOpen at the same time, e.g. one
  // thread could be in process of inserting some handle while this
  // thread is looking up another handle.
  std::lock_guard lk(mutex);
  auto itr = userpf_device_map.find(handle);
  if (itr != userpf_device_map.end())
    return (*itr).second.lock();
  return nullptr;
}

std::shared_ptr<device>
get_userpf_device(device::handle_type handle, device::id_type id)
{
  // Check device map cache
  if (auto device = get_userpf_device(handle)) {
    if (device->get_device_id() != id)
        throw std::runtime_error("get_userpf_device: id mismatch");
    return device;
  }

  // Construct a new device object and insert in map.
  auto device = instance().get_userpf_device(handle,id);
  std::lock_guard lk(mutex);
  userpf_device_map[handle] = device;  // create or replace
  return device;
}

std::shared_ptr<device>
get_mgmtpf_device(device::id_type id)
{
  // Check cache
  std::lock_guard lk(mutex);
  auto itr = mgmtpf_device_map.find(id);
  if (itr != mgmtpf_device_map.end())
    if (auto device = (*itr).second.lock())
      return device;

  // Construct a new device object and insert in map
  auto device = instance().get_mgmtpf_device(id);
  mgmtpf_device_map[id] = device;
  return device;
}

std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
get_bdf_info(device::id_type id, bool is_user)
{
  return instance().get_bdf_info(id, is_user);
}

std::pair<device::id_type, device::id_type>
get_total_devices(bool is_user)
{
  return instance().get_total_devices(is_user);
}

device::id_type
get_device_id(const std::string& str)
{
  return instance().get_device_id(str);
}

system::monitor_access_type
get_monitor_access_type()
{
  return instance().get_monitor_access_type();
}

void
program_plp(const device* dev, const std::vector<char> &buffer, bool force)
{
  instance().program_plp(dev, buffer, force);
}

size_t
get_device_factory_ready(bool user)
{
  return instance().get_num_dev_ready(user);
}

size_t
get_device_factory_total(bool user)
{
  return instance().get_num_dev_total(user);
}

std::shared_ptr<device_factory>
get_device_factory(unsigned int index, bool user)
{
  return instance().get_device(index, user);
}

void
register_device_list(const std::vector<std::shared_ptr<xrt_core::device_factory>>& devlist)
{
  for (auto device : devlist) {
    if (device->is_mgmt()) {
      if (device->is_ready())
        mgmt_ready_list.push_back(std::move(device));        
      else
        mgmt_nonready_list.push_back(std::move(device));        
    }
    else {
      if (device->is_ready())
        user_ready_list.push_back(std::move(device));
      else
        user_nonready_list.push_back(std::move(device));
    }
  }
}

} // xrt_core


