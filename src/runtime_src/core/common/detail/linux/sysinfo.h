// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

// 3rd Party Library - Include Files
#include <boost/property_tree/ptree.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/ini_parser.hpp>

// System - Include Files
#include <gnu/libc-version.h>
#include <sys/utsname.h>
#include <thread>

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
static std::string 
machine_info()
{
  std::string model("unknown");
  std::ifstream stream(MACHINE_NODE_PATH);
  if (stream.good())
    std::getline(stream, model);
  return model;
}

static boost::property_tree::ptree
glibc_info()
{
  boost::property_tree::ptree pt;
  pt.put("name", "glibc");
  pt.put("version", gnu_get_libc_version());
  return pt;
}

static std::string 
processor_name()
{
  std::ifstream cpuinfo("/proc/cpuinfo");
  std::string line;
  std::string model_name = "Unknown";

  if (cpuinfo.is_open()) {
    while (std::getline(cpuinfo, line)) {
      if (line.rfind("model name", 0) != 0) { // Check if line starts with "model name"
        continue;
      }
      size_t colon_pos = line.find(":");
      if (colon_pos != std::string::npos) {
        model_name = line.substr(colon_pos + 2); // Extract substring after ": "
        break;
      }
    }
  }
  return model_name;
}
} //end anonymous namespace


namespace xrt_core::sysinfo::detail {

void
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
  std::ifstream ifs("/etc/os-release");
  if (!ifs.good())
    return;

	boost::property_tree::ptree opt;
  boost::property_tree::ini_parser::read_ini(ifs, opt);
	std::string val = opt.get<std::string>("PRETTY_NAME", "");
	if (val.empty())
	  return;
	      
	if ((val.front() == '"') && (val.back() == '"')) {
	  val.erase(0, 1);
	  val.erase(val.size()-1);
	}
	pt.put("distribution", val);

  // BIOS info
  std::string bios_vendor("unknown");
  std::string bios_version("unknown");
  std::ifstream bios_stream("/sys/class/dmi/id/bios_vendor");
  if (bios_stream.is_open()) {
    getline(bios_stream, bios_vendor);
  }
  pt.put("bios_vendor", bios_vendor);

  std::ifstream ver_stream("/sys/class/dmi/id/bios_version");
  if (ver_stream.is_open()) {
    getline(ver_stream, bios_version);
  }
  pt.put("bios_version", bios_version);

  pt.put("model", machine_info());
  pt.put("cores", std::thread::hardware_concurrency());
  pt.put("memory_bytes", (boost::format("0x%lx") % (sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE))).str());
  boost::property_tree::ptree ptLibInfo;
  ptLibInfo.push_back( {"", glibc_info()} );
  pt.put_child("libraries", ptLibInfo);

  char hostname[256] = {0};
  gethostname(hostname, 256);
  std::string hn(hostname);
  pt.put("hostname", hn);

  pt.put("processor", processor_name());
}

} //xrt_core::sysinfo
