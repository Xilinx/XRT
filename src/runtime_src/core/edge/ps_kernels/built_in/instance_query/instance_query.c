// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#include "xrt/xrt_kernel.h"

#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <set>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
namespace pt = boost::property_tree;

#ifdef __cplusplus
extern "C" {
#endif

#include "sk_types.h"

// User private data structure container (context object) definition
class xrtHandles : public pscontext
{
public:
};

void
parse_file(pt::ptree& pt, const std::string& file_path, const bool is_dict = true, const std::string& delimiter = ":")
{
  std::ifstream stream;
  stream.open(file_path);
  std::string stream_data;
  // Read through the process status data and add it into the ptree
  while (std::getline(stream, stream_data)) {
    auto pos = stream_data.find(delimiter);
    // If no colon is found skip the value
    if (pos == std::string::npos)
      continue;

    // Remove any tabs
    std::replace(stream_data.begin(), stream_data.end(), '\t', ' ');
    auto key = stream_data.substr(0, pos);
    auto value = stream_data.substr(pos + 1, stream_data.size());

    // Remove any extra spaces
    boost::algorithm::trim(key);
    boost::algorithm::trim(value);
    // Generate node for data
    pt::ptree new_node;
    if (is_dict)
      pt.put(key, value);
    else {
      new_node.put("name", key);
      new_node.put("value", value);
      pt.push_back(std::make_pair("", new_node));
    }
  }
}

void
add_schema(pt::ptree &pt)
{
    pt::ptree schema_pt;
    schema_pt.put("schema", "JSON");
    schema_pt.put("major", "1");
    schema_pt.put("minor", "0");
    schema_pt.put("patch", "0");
    // Add the schema node
    pt.add_child("schema_version", schema_pt);
}

void
get_os_release(pt::ptree &pt)
{
    pt::ptree os_pt;
    struct utsname sysinfo;
    if (!uname(&sysinfo)) {
      os_pt.put("sysname",   sysinfo.sysname);
      os_pt.put("release",   sysinfo.release);
      os_pt.put("version",   sysinfo.version);
      os_pt.put("machine",   sysinfo.machine);
    }

    // Extract a single node for the distribution data
    std::ifstream ifs("/etc/os-release");
    if (ifs.good()) {
      pt::ptree opt;
      pt::ini_parser::read_ini(ifs, opt);
      std::string val = opt.get<std::string>("PRETTY_NAME", "");
      if (val.length()) {
        if ((val.front() == '"') && (val.back() == '"')) {
          val.erase(0, 1);
          val.erase(val.size()-1);
        }
        os_pt.put("distribution", val);
      }
    }
    ifs.close();

    std::string model("unknown");
    std::ifstream stream("/proc/device-tree/model");
    if (stream.good()) {
      std::getline(stream, model);
      stream.close();
    }
    os_pt.put("model", model);

    os_pt.put("cores", std::thread::hardware_concurrency());
    os_pt.put("address_spaces", (boost::format("0x%lx") % (sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE))).str());
    os_pt.put("available_memory", "????"); // TODO

    pt.add_child("os", os_pt);
}

void
get_instance_process_status( const std::string& pid,
                    pt::ptree &pt)
{
  // Format the file path to point to a process status area
  std::string file_path = "/proc/" + pid + "/status";
  pt::ptree data_pt;
  parse_file(data_pt, file_path, false);

  // Remove undesired nodes
  // Lambda to detect unwanted node names
  auto inv_name = [](const std::pair<std::string, pt::ptree>& item) {
    // The list of node names that should be removed
    static std::set<std::string> invalid_names = {"Name"};
    return invalid_names.find(item.second.get<std::string>("name")) != invalid_names.end(); 
  };

  // Shorthand for find_if function
  auto find_invalid = [&](pt::ptree& p_pt) {
    return std::find_if(p_pt.begin(), p_pt.end(), inv_name);
  };

  // Loop until all invalid names are removed
  for (auto it = find_invalid(data_pt); it != data_pt.end(); it = find_invalid(data_pt))
    data_pt.erase(it);

  pt.add_child("process_info", data_pt);
}

// If unknown entries are detected they will be added into the output
// ptree as listed
static void
filter_ptree_contents(pt::ptree& output, const pt::ptree& input, std::map<std::string, std::string>& filter)
{
  for (const auto& item : input) {
    const auto& it = filter.find(item.first);
    if (it != filter.end())
      output.put(it->second, item.second.data());
    else
      output.put(item.first, item.second.data());
  }
}

void
get_instance_status(const std::string& file,
                    pt::ptree &pt)
{
  std::string pid_path = file + "/status";
  pt::ptree status_pt;
  parse_file(status_pt, pid_path);

  static std::map<std::string, std::string> name_map = {
      { "PID", "pid"}
  };
  filter_ptree_contents(pt, status_pt, name_map);
}

void
get_instance_info(  const std::string& file,
                    pt::ptree &pt)
{
  std::string info_path = file + "/cu_info";
  pt::ptree info_pt;
  parse_file(info_pt, info_path);

  static std::map<std::string, std::string> name_map = {
      { "Kernel name",           "kernel"               },
      { "Instance(CU) name",     "name"                 },
      { "CU address",            "cu_address"           },
      { "CU index",              "cu_index"             },
      { "Protocol",              "protocol"             },
      { "Interrupt cap",         "interrupt_compatible" },
      { "SW Resettable",         "resettable"           },
      { "Number of arguments",   "argument_count"       }
  };
  filter_ptree_contents(pt, info_pt, name_map);
}

__attribute__((visibility("default")))
int instance_query(char *output, int count, struct xrtHandles *xrtHandle)
{
  openlog("new_kernel_source", LOG_PID | LOG_CONS | LOG_NDELAY, LOG_NEWS);
  syslog(LOG_INFO, "%s: Started new kernel\n", __func__);

  boost::filesystem::path p("/sys/devices/platform/ert_hw/");
  boost::filesystem::directory_iterator dir(p);
  pt::ptree all_data;
  add_schema(all_data);
  get_os_release(all_data);

  pt::ptree all_ps_data;
  while (dir != boost::filesystem::directory_iterator()) {
    // Get a copy of the current directories name.
    // Getting a reference cause a undesired change when the
    // the directory updates
    const auto path = (*dir).path().string();
    const auto filename = (*dir).path().filename().string();
    // After storing the path and filename increment the directory
    dir++;
    syslog(LOG_INFO, "%s: Testing %s\n", __func__, path.c_str());

    // Skip over non matching directories
    if (filename.find("SCU") == std::string::npos)
        continue;

    syslog(LOG_INFO, "%s: Discovered %s\n", __func__, path.c_str());

    // Get the PID and full name of the PS kernel instance
    pt::ptree process_pt;
    get_instance_status(path, process_pt);
    get_instance_info(path, process_pt);
    get_instance_process_status(process_pt.get<std::string>("pid"), process_pt);

    // Use the PID as the names can be unreliable. Let the host stitch together
    // the kernel and instance names to verify what is what. This kernel should
    // just hand back as much data as required
    all_ps_data.push_back(std::make_pair("", process_pt));
  }
  all_data.add_child("ps_kernel_instances", all_ps_data);

  // Generate JSON output
  std::stringstream ss;
  pt::json_parser::write_json(ss, all_data, false);

  // Write into output buffer
  snprintf(output, count, "%s", ss.str().c_str());

  syslog(LOG_INFO, "%s: Stopped new kernel\n", __func__);
  closelog();

  return 0;
}

#ifdef __cplusplus
}
#endif

