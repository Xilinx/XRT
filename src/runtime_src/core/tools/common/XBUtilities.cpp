// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
#include "XBUtilities.h"
#include "XBUtilitiesCore.h"

// Local - Include Files
#include "common/error.h"
#include "common/info_vmr.h"
#include "common/utils.h"
#include "common/message.h"
#include "common/system.h"
#include "common/sysinfo.h"
#include "common/smi.h"

// 3rd Party Library - Include Files
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/tokenizer.hpp>

// System - Include Files
#include <iostream>
#include <map>
#include <regex>


#ifdef _WIN32

# pragma warning( disable : 4189 4100 )
# pragma comment(lib, "Ws2_32.lib")
/* need to link the lib for the following to work */
# define be32toh ntohl
#else
# include <unistd.h> // SUDO check
#endif

#define ALIGN(x, a)     (((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a)    ((char *)(ALIGN((unsigned long long)(p), (a))))
#define GET_CELL(p)     (p += 4, *((const uint32_t *)(p-4)))

// ------ C O N S T A N T   V A R I A B L E S ---------------------------------
static const uint32_t FDT_BEGIN_NODE = 0x1;
static const uint32_t FDT_PROP = 0x3;
static const uint32_t FDT_END = 0x9;

// ------ L O C A L  F U N C T I O N S  A N D  S T R U C T S ------------------
struct fdt_header {
  uint32_t magic;
  uint32_t totalsize;
  uint32_t off_dt_struct;
  uint32_t off_dt_strings;
  uint32_t off_mem_rsvmap;
  uint32_t version;
  uint32_t last_comp_version;
  uint32_t boot_cpuid_phys;
  uint32_t size_dt_strings;
  uint32_t size_dt_struct;
};

namespace xq = xrt_core::query;

// ------ F U N C T I O N S ---------------------------------------------------

std::string
XBUtilities::Timer::format_time(std::chrono::duration<double> duration) 
{
  auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
  auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);

  std::string formatted_time;
  if (hours.count() != 0) 
    formatted_time += std::to_string(hours.count()) + "h ";

  if (hours.count() != 0 || minutes.count() != 0) 
    formatted_time += std::to_string(minutes.count() % 60) + "m ";

  formatted_time += std::to_string(seconds.count() % 60) + "s";

  return formatted_time;
}

boost::property_tree::ptree
XBUtilities::get_available_devices(bool inUserDomain)
{
  xrt_core::device_collection deviceCollection;
  collect_devices(std::set<std::string> {"_all_"}, inUserDomain, deviceCollection);
  boost::property_tree::ptree pt;
  for (const auto & device : deviceCollection) {
    boost::property_tree::ptree pt_dev;
    pt_dev.put("bdf", xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device)));

    const auto device_class = xrt_core::device_query_default<xrt_core::query::device_class>(device, xrt_core::query::device_class::type::alveo);
    pt_dev.put("device_class", xrt_core::query::device_class::enum_to_str(device_class));

    //user pf doesn't have mfg node. Also if user pf is loaded, it means that the card is not is mfg mode
    const auto is_mfg = xrt_core::device_query_default<xrt_core::query::is_mfg>(device, false);

    //if factory mode
    if (is_mfg) {
      auto mGoldenVer = xrt_core::device_query<xrt_core::query::mfg_ver>(device);
      std::string vbnv = "xilinx_" + xrt_core::device_query<xrt_core::query::board_name>(device) + "_GOLDEN_"+ std::to_string(mGoldenVer);
      pt_dev.put("vbnv", vbnv);
      pt_dev.put("id", "n/a");
      pt_dev.put("instance","n/a");
    }
    else {
      switch (device_class) {
      case xrt_core::query::device_class::type::alveo:
        pt_dev.put("vbnv", xrt_core::device_query<xrt_core::query::rom_vbnv>(device));
        break;
      case xrt_core::query::device_class::type::ryzen:
        pt_dev.put("name", xrt_core::device_query<xrt_core::query::rom_vbnv>(device));
        break;
      }
      
      try { //1RP
        pt_dev.put("id", xrt_core::query::rom_time_since_epoch::to_string(xrt_core::device_query<xrt_core::query::rom_time_since_epoch>(device)));
      }
      catch(...) {
        // The id wasn't added
      }

      try { //2RP
        auto logic_uuids = xrt_core::device_query<xrt_core::query::logic_uuids>(device);
        if (!logic_uuids.empty())
          pt_dev.put("id", xrt_core::query::interface_uuids::to_uuid_upper_string(logic_uuids[0]));
      }
      catch(...) {
        // The id wasn't added
      }

      try {
        const auto fw_ver = xrt_core::device_query_default<xq::firmware_version>(device, {0,0,0,0});
        std::string version = "N/A";
        if (fw_ver.major != 0 || fw_ver.minor != 0 || fw_ver.patch != 0 || fw_ver.build != 0) {
          version = boost::str(boost::format("%u.%u.%u.%u")
            % fw_ver.major % fw_ver.minor % fw_ver.patch % fw_ver.build);
        }
        pt_dev.put("firmware_version", version);
      }
      catch(...) {
        // The firmware wasn't added
      }

      try {
        auto instance = xrt_core::device_query<xrt_core::query::instance>(device);
        std::string pf = device->is_userpf() ? "user" : "mgmt";
        pt_dev.put("instance",boost::str(boost::format("%s(inst=%d)") % pf % instance));
      }
      catch(const xrt_core::query::exception&) {
          // The instance wasn't added
      }

    }
    pt_dev.put("is_ready", xrt_core::device_query_default<xrt_core::query::is_ready>(device, true));

    pt.push_back(std::make_pair("", pt_dev));
  }
  return pt;
}

/*
 * currently edge supports only one device
 */
static uint16_t
deviceId2index()
{
  return 0;
}

std::string
XBUtilities::str_available_devs(bool _inUserDomain)
{
  //gather available devices for user to pick from
  std::stringstream available_devs;
  available_devs << "\n Available devices:\n";
  boost::property_tree::ptree available_devices = XBUtilities::get_available_devices(_inUserDomain);
  for (auto& kd : available_devices) {
    boost::property_tree::ptree& dev = kd.second;
    if (boost::iequals(dev.get<std::string>("device_class"), xrt_core::query::device_class::enum_to_str(xrt_core::query::device_class::type::alveo)))
      available_devs << boost::format("  [%s] : %s\n") % dev.get<std::string>("bdf") % dev.get<std::string>("vbnv");
    if (boost::iequals(dev.get<std::string>("device_class"), xrt_core::query::device_class::enum_to_str(xrt_core::query::device_class::type::ryzen)))
      available_devs << boost::format("  [%s] : %s\n") % dev.get<std::string>("bdf") % dev.get<std::string>("name");
  }
  return available_devs.str();
}

/*
 * Parse the bdf passed in by the user to an index
 * DDDD:BB:DD.F where domain and function are optional
 */
static uint16_t
bdf2index(const std::string& bdfstr, bool _inUserDomain)
{
  if (!std::regex_match(bdfstr,std::regex("[A-Za-z0-9:.]+")))
    throw std::runtime_error("Invalid BDF format. Please specify valid BDF" + XBUtilities::str_available_devs(_inUserDomain));

  std::vector<std::string> tokens;
  boost::split(tokens, bdfstr, boost::is_any_of(":"));
  int radix = 16;
  uint16_t domain = 0;
  uint16_t bus = 0;
  uint16_t dev = 0;
  uint16_t func = std::numeric_limits<uint16_t>::max();

  // check if we have 2-3 tokens: domain, bus, device.function
  // domain is optional
  if (tokens.size() <= 1 || tokens.size() > 3)
    throw std::runtime_error(boost::str(boost::format("Invalid BDF '%s'. Please spcify the BDF using 'DDDD:BB:DD.F' format") % bdfstr) + XBUtilities::str_available_devs(_inUserDomain));

  std::reverse(std::begin(tokens), std::end(tokens));

try {
    //check if func was specified. func is optional
    auto pos_of_func = tokens[0].find('.');
    if (pos_of_func != std::string::npos) {
      dev = static_cast<uint16_t>(std::stoi(std::string(tokens[0].substr(0, pos_of_func)), nullptr, radix));
      func = static_cast<uint16_t>(std::stoi(std::string(tokens[0].substr(pos_of_func+1)), nullptr, radix));
    }
    else
      dev = static_cast<uint16_t>(std::stoi(std::string(tokens[0]), nullptr, radix));

    bus = static_cast<uint16_t>(std::stoi(std::string(tokens[1]), nullptr, radix));

    // domain is not mandatory if it is "0000"
    if(tokens.size() > 2)
      domain = static_cast<uint16_t>(std::stoi(std::string(tokens[2]), nullptr, radix));
  } catch (const std::invalid_argument&) {
    throw std::runtime_error(boost::str(boost::format("Invalid BDF '%s'") % bdfstr) + XBUtilities::str_available_devs(_inUserDomain));
  }

  // Iterate through the available devices to find a BDF match
  // This must not open any devices! Doing do would slow down the software
  // quite a bit and cause other undesirable side affects
auto devices = _inUserDomain ? xrt_core::get_total_devices(true).first : xrt_core::get_total_devices(false).first;
  for (decltype(devices) i = 0; i < devices; i++) {
    auto bdf = xrt_core::get_bdf_info(i, _inUserDomain);
    //if the user specifies func, compare
    //otherwise safely ignore
    auto cmp_func = [bdf](uint16_t func)
    {
      if (func != std::numeric_limits<uint16_t>::max())
        return func == std::get<3>(bdf);
      return true;
    };

    if (domain == std::get<0>(bdf) && bus == std::get<1>(bdf) && dev == std::get<2>(bdf) && cmp_func(func))
      return static_cast<uint16_t>(i);
  }

  throw std::runtime_error(boost::str(boost::format("Specified device BDF '%s' not found") % bdfstr) + XBUtilities::str_available_devs(_inUserDomain));
}

static std::shared_ptr<xrt_core::device>
get_device_internal(xrt_core::device::id_type index, bool in_user_domain)
{
  static std::mutex mutex;
  std::lock_guard guard(mutex);

  if (in_user_domain) {
    static std::vector<std::shared_ptr<xrt_core::device>> user_devices(xrt_core::get_total_devices(true).first, nullptr);
  
    if (user_devices.size() <= index )
      throw std::runtime_error("no device present with index " + std::to_string(index));
    
    if (!user_devices[index])
      user_devices[index] = xrt_core::get_userpf_device(index);

    return user_devices[index];
  }

  static std::vector<std::shared_ptr<xrt_core::device>> mgmt_devices(xrt_core::get_total_devices(false).first, nullptr);
  
  if (mgmt_devices.size() <= index )
    throw std::runtime_error("no device present with index " + std::to_string(index));

  if (!mgmt_devices[index])
    mgmt_devices[index] = xrt_core::get_mgmtpf_device(index);

  return mgmt_devices[index];
}

/*
 * Map the string passed in by the user to a valid index
 * Supports pcie and edge devices
 */
static uint16_t
str2index(const std::string& str, bool _inUserDomain)
{
  //throw an error if no devices are present
  uint64_t devices = _inUserDomain ? xrt_core::get_total_devices(true).first : xrt_core::get_total_devices(false).first;
  if (devices == 0)
    throw std::runtime_error("No devices found");
  try {
    int idx(boost::lexical_cast<int>(str));
    auto device = get_device_internal(idx, _inUserDomain);

    auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);
    // if the bdf is zero, we are dealing with an edge device
    if (std::get<0>(bdf) == 0 && std::get<1>(bdf) == 0 && std::get<2>(bdf) == 0 && std::get<3>(bdf) == 0)
      return deviceId2index();
  } catch (...) {
    /* not an edge device so safe to ignore this error */
  }
  return bdf2index(str, _inUserDomain);
}

void
XBUtilities::xrt_version_cmp(bool isUserDomain)
{
  boost::property_tree::ptree pt_xrt;
  xrt_core::sysinfo::get_xrt_info(pt_xrt);
  const boost::property_tree::ptree empty_ptree;

  std::string xrt_version = pt_xrt.get<std::string>("version", "<unknown>");
  const boost::property_tree::ptree& available_drivers = pt_xrt.get_child("drivers", empty_ptree);
  const std::string expected_drv_name = isUserDomain ? "xocl" : "xclmgmt";
  for(const auto& drv : available_drivers) {
    const boost::property_tree::ptree& driver = drv.second;
    const std::string drv_name = driver.get<std::string>("name", "<unknown>");
    const std::string drv_version = driver.get<std::string>("version", "<unknown>");
    if (drv_name.compare(expected_drv_name) == 0 && drv_version.compare("unknown") != 0 && xrt_version.compare(drv_version) != 0) {
      const auto & warnMsg = boost::str(boost::format("WARNING: Unexpected %s version (%s) was found. Expected %s, to match XRT tools.") % expected_drv_name % drv_version % xrt_version);
      std::cout << warnMsg << std::endl;
    }
  }
}

void
XBUtilities::collect_devices( const std::set<std::string> &_deviceBDFs,
                              bool _inUserDomain,
                              xrt_core::device_collection &_deviceCollection)
{
  // -- If the collection is empty then do nothing
  if (_deviceBDFs.empty())
    return;

  // -- Collect all of devices if the "all" option is used...anywhere in the collection
  if (_deviceBDFs.find("_all_") != _deviceBDFs.end()) {
    xrt_core::device::id_type total = 0;
    try {
      // If there are no devices in the server a runtime exception is thrown in  mgmt.cpp probe()
      total = (xrt_core::device::id_type) xrt_core::get_total_devices(_inUserDomain /*isUser*/).first;
    } catch (...) {
      /* Do nothing */
    }

    // No devices found
    if (total == 0)
      return;

    // Now collect the devices and add them to the collection
    for(xrt_core::device::id_type index = 0; index < total; ++index) {
      try {
        _deviceCollection.push_back(get_device_internal(index, _inUserDomain));
      } catch (...) {
        /* If the device is not available, quietly ignore it
           Use case: when a device is being reset in parallel */
      }

    }

    return;
  }

  // -- Collect the devices by name
  for (const auto & deviceBDF : _deviceBDFs) {
    auto index = str2index(deviceBDF, _inUserDomain);         // Can throw
    _deviceCollection.push_back(get_device_internal(index, _inUserDomain));
  }
}

  static void 
  check_versal_boot(const std::shared_ptr<xrt_core::device> &device)
  {
    std::vector<std::string> warnings;

    try {
      const auto is_default = xrt_core::vmr::get_vmr_status(device.get(), xrt_core::vmr::vmr_status_type::has_fpt);
      if (!is_default)
        warnings.push_back("Versal Platform is NOT migrated");
    } catch (const xrt_core::error& e) {
      warnings.push_back(e.what());
    }

    try {
      const auto is_default = xrt_core::vmr::get_vmr_status(device.get(), xrt_core::vmr::vmr_status_type::boot_on_default);
      if (!is_default)
        warnings.push_back("Versal Platform is NOT in default boot");
    } catch (const xrt_core::error& e) {
      warnings.push_back(e.what());
    }

    try {
      const std::string unavail = "N/A";
      const std::string zeroes = "0.0.0";
      auto cur_ver = xrt_core::device_query_default<xrt_core::query::hwmon_sdm_active_msp_ver>(device, unavail);
      auto exp_ver = xrt_core::device_query_default<xrt_core::query::hwmon_sdm_target_msp_ver>(device, unavail);
      cur_ver = (boost::equals(cur_ver, zeroes)) ? unavail : cur_ver;
      exp_ver = (boost::equals(exp_ver, zeroes)) ? unavail : exp_ver;
      if ((boost::equals(cur_ver, unavail) || boost::equals(exp_ver, unavail)) && !boost::equals(cur_ver, exp_ver))
        warnings.push_back("SC version data missing. Upgrade your shell");
      else if (!boost::equals(cur_ver, exp_ver))
        warnings.push_back(boost::str(boost::format("Invalid SC version. Expected: %s Current: %s") % exp_ver % cur_ver));
    } catch (const xrt_core::error& e) {
      warnings.push_back(e.what());
    }

    if (warnings.empty())
      return;

    const std::string star_line = "***********************************************************";

    std::cout << star_line << "\n";
    std::cout << "*        WARNING          WARNING          WARNING        *\n";

    // Print all warnings
    for (const auto& warning : warnings) {
      // Subtract the:
      // 1. Side stars
      // 2. Single space next to the side star
      const size_t available_space = star_line.size() - 2 - 2;
      // Account for strings who are larger than the star line
      size_t warning_index = 0;
      while (warning_index < warning.size()) {
        // Extract the largest possible string from the warning
        const auto warning_msg = warning.substr(warning_index, available_space);
        // Update the index so the next substring is valid
        warning_index += warning_msg.size();
        const auto side_spaces = available_space - warning_msg.size();
        // The left side should be larger than the right if there is an imbalance
        const size_t left_spaces = (side_spaces % 2 == 0) ? side_spaces / 2 : (side_spaces / 2) + 1;
        const size_t right_spaces = side_spaces / 2;
        std::cout << "* " << std::string(left_spaces, ' ') << warning_msg << std::string(right_spaces, ' ') << " *\n";
      }
    }

    std::cout << star_line << "\n";
  }

  std::shared_ptr<xrt_core::device>
  XBUtilities::get_device( const std::string &deviceBDF, bool in_user_domain)
  {
    // -- If the deviceBDF is empty then do nothing
    if (deviceBDF.empty())
      throw std::runtime_error("Please specify a device using --device option" + XBUtilities::str_available_devs(in_user_domain));

    // -- Collect the devices by name
    auto index = str2index(deviceBDF, in_user_domain);    // Can throw
    std::shared_ptr<xrt_core::device> device;
    device = get_device_internal(index, in_user_domain);

    if (xrt_core::device_query_default<xq::is_versal>(device, false))
      check_versal_boot(device);

    return device;
  }

static std::string
deviceMapping(const xrt_core::query::device_class::type type)
{
  switch (type) {
  case xrt_core::query::device_class::type::alveo:
    return "alveo";
  case xrt_core::query::device_class::type::ryzen:
    return "aie";
  }

  return "";
}

std::string
XBUtilities::get_device_class(const std::string &deviceBDF, bool in_user_domain)
{
  if (deviceBDF.empty()) 
    return "";

  std::shared_ptr<xrt_core::device> device = get_device(boost::algorithm::to_lower_copy(deviceBDF), in_user_domain);
  auto device_class = xrt_core::device_query_default<xrt_core::query::device_class>(device, xrt_core::query::device_class::type::alveo);
  return deviceMapping(device_class);
}

void
XBUtilities::can_proceed_or_throw(const std::string& info, const std::string& error)
{
  std::cout << info << "\n";
  if (!XBUtilities::can_proceed(getForce()))
    throw xrt_core::system_error(ECANCELED, error);
}

void
XBUtilities::print_exception(const std::system_error& e)
{
  try {
    // Remove the type of error from the message.
    const std::string msg = std::regex_replace(e.what(), std::regex(std::string(": ") + e.code().message()), "");

    if ((!msg.empty()) && (!boost::icontains(msg, "operation canceled")))
      std::cerr << boost::format("ERROR: %s\n") % msg;
  }
  catch (const std::exception&)
  {
    // exception can occur while formatting message, print normal message
    std::cerr << e.what() << std::endl;
  }
}

std::vector<char>
XBUtilities::get_axlf_section(const std::string& filename, axlf_section_kind kind)
{
  std::ifstream in(filename, std::ios::binary);
  if (!in.is_open())
    throw std::runtime_error(boost::str(boost::format("Can't open %s") % filename));

  // Read axlf from dsabin file to find out number of sections in total.
  axlf a;
  size_t sz = sizeof (axlf);
  in.read(reinterpret_cast<char *>(&a), sz);
  if (!in.good())
    throw std::runtime_error(boost::str(boost::format("Can't read axlf from %s") % filename));

  // Reread axlf from dsabin file, including all sections headers.
  // Sanity check for number of sections coming from user input file
  if (a.m_header.m_numSections > 10000)
    throw std::runtime_error("Incorrect file passed in");

  sz = sizeof (axlf) + sizeof (axlf_section_header) * (a.m_header.m_numSections - 1);

  std::vector<char> top(sz);
  in.seekg(0);
  in.read(top.data(), sz);
  if (!in.good())
    throw std::runtime_error(boost::str(boost::format("Can't read axlf and section headers from %s") % filename));

  const axlf *ap = reinterpret_cast<const axlf *>(top.data());
  auto section = ::xclbin::get_axlf_section(ap, kind);
  if (!section)
    throw std::runtime_error("Section not found");

  std::vector<char> buf(section->m_sectionSize);
  in.seekg(section->m_sectionOffset);
  in.read(buf.data(), section->m_sectionSize);

  return buf;
}

std::vector<std::string>
XBUtilities::get_uuids(const void *dtbuf)
{
  std::vector<std::string> uuids;
  struct fdt_header *bph = (struct fdt_header *)dtbuf;
  uint32_t version = be32toh(bph->version);
  uint32_t off_dt = be32toh(bph->off_dt_struct);
  const char *p_struct = (const char *)dtbuf + off_dt;
  uint32_t off_str = be32toh(bph->off_dt_strings);
  const char *p_strings = (const char *)dtbuf + off_str;
  const char *p, *s;
  uint32_t tag;
  int sz;

  p = p_struct;
  uuids.clear();
  while ((tag = be32toh(GET_CELL(p))) != FDT_END) {
    if (tag == FDT_BEGIN_NODE) {
      s = p;
      p = PALIGN(p + strlen(s) + 1, 4);
      continue;
    }
    if (tag != FDT_PROP)
      continue;

    sz = be32toh(GET_CELL(p));
    s = p_strings + be32toh(GET_CELL(p));
    if (version < 16 && sz >= 8)
      p = PALIGN(p, 8);

    if (!strcmp(s, "logic_uuid")) {
      uuids.insert(uuids.begin(), std::string(p));
    }
    else if (!strcmp(s, "interface_uuid")) {
      uuids.push_back(std::string(p));
    }

    p = PALIGN(p + sz, 4);
  }
  return uuids;
}

static const std::map<std::string, xrt_core::query::reset_type> reset_map = {
    { "hot", xrt_core::query::reset_type(xrt_core::query::reset_key::hot, "HOT Reset", "", "mgmt_reset", "Please make sure xocl driver is unloaded.", "1") },
    { "kernel", xrt_core::query::reset_type(xrt_core::query::reset_key::kernel, "KERNEL Reset", "", "mgmt_reset", "Please make sure no application is currently running.", "2") },
    { "ert", xrt_core::query::reset_type(xrt_core::query::reset_key::ert, "ERT Reset", "", "mgmt_reset", "", "3") },
    { "ecc", xrt_core::query::reset_type(xrt_core::query::reset_key::ecc, "ECC Reset", "", "ecc_reset", "", "4") },
    { "soft-kernel", xrt_core::query::reset_type(xrt_core::query::reset_key::soft_kernel, "SOFT KERNEL Reset", "", "mgmt_reset", "", "5") },
    { "aie", xrt_core::query::reset_type(xrt_core::query::reset_key::aie, "AIE Reset", "", "mgmt_reset", "", "6") },
    { "user", xrt_core::query::reset_type(xrt_core::query::reset_key::user, "HOT Reset", "", "", "", "") },
  };

xrt_core::query::reset_type
XBUtilities::str_to_reset_obj(const std::string& str)
{
  auto it = reset_map.find(str);
  if (it != reset_map.end())
    return it->second;
  throw xrt_core::error(str + " is invalid. Please specify a valid reset type");
}

std::string
XBUtilities::string_to_UUID(std::string str)
{
  //make sure that a UUID is passed in
  assert(str.length() == 32);
  std::string uuid = "";
  //positions to insert hyphens
  //before: 00000000000000000000000000000000
  std::vector<int> pos = {8, 4, 4, 4};
  //before: 00000000-0000-0000-0000-000000000000

  for(auto const p : pos) {
    std::string token = str.substr(0, p);
    boost::to_upper(token);
    uuid.append(token + "-");
    str.erase(0, p);
  }
  boost::to_upper(str);
  uuid.append(str);

  return uuid;
}

static const std::map<uint64_t, std::string> oemid_map = {
  {0x10da, "Xilinx"},
  {0x02a2, "Dell"},
  {0x12a1, "IBM"},
  {0xb85c, "HP"},
  {0x2a7c, "Super Micro"},
  {0x4a66, "Lenovo"},
  {0xbd80, "Inspur"},
  {0x12eb, "Amazon"},
  {0x2b79, "Google"}
};

std::string
XBUtilities::parse_oem_id(const std::string& oemid)
{
  uint64_t oem_id_val = 0;
  std::stringstream ss;

  try {
    oem_id_val = std::stoul(oemid, nullptr, 16);
  } catch (const std::exception&) {
    //failed to parse oemid to hex value, ignore erros and print original value
  }

  auto oemstr = oemid_map.find(oem_id_val);
  return oemstr != oemid_map.end() ? oemstr->second : "N/A";
}

static const std::map<std::string, std::string> clock_map = {
  {"DATA_CLK", "Data"},
  {"KERNEL_CLK", "Kernel"},
  {"SYSTEM_CLK", "System"},
};

std::string
XBUtilities::parse_clock_id(const std::string& id)
{
  auto clock_str = clock_map.find(id);
  if (clock_str != clock_map.end())
    return clock_str->second;

  throw xrt_core::error(std::errc::invalid_argument);
}

uint64_t
XBUtilities::string_to_base_units(std::string str, const unit& conversion_unit)
{
  boost::algorithm::trim(str);

  if(str.empty())
    throw xrt_core::error(std::errc::invalid_argument);

  int factor;
  switch(conversion_unit) {
    case unit::bytes :
      factor = 1024;
      break;
    case unit::Hertz :
      factor = 1000;
      break;
    default :
      throw xrt_core::error(std::errc::invalid_argument);
  }

  std::string units = "B";
  if(std::isalpha(str.back())) {
    units = str.back();
    str.pop_back();
  }

  uint64_t unit_value = 0;
  boost::to_upper(units);
  if(units.compare("B") == 0)
    unit_value = 1;
  else if(units.compare("K") == 0)
    unit_value = factor;
  else if(units.compare("M") == 0)
    unit_value = factor * factor;
  else if(units.compare("G") == 0)
    unit_value = factor * factor * factor;
  else
    throw xrt_core::error(std::errc::invalid_argument);

  boost::algorithm::trim(str);
  uint64_t size = 0;
  try {
    size = std::stoll(str);
  }
  catch (const std::exception&) {
    //out of range, invalid argument ex
    throw xrt_core::error(std::errc::invalid_argument);
  }

  size *= unit_value;
  return size;
}

std::string
XBUtilities::
get_xrt_pretty_version()
{
  std::stringstream ss;
  boost::property_tree::ptree pt_xrt;
  xrt_core::sysinfo::get_xrt_info(pt_xrt);
  const boost::property_tree::ptree available_devices = XBUtilities::get_available_devices(true);
  XBUtilities::fill_xrt_versions(pt_xrt, ss, available_devices);
  return ss.str();
}

void 
XBUtilities::
fill_xrt_versions(const boost::property_tree::ptree& pt_xrt, 
                 std::stringstream& output, 
                 const boost::property_tree::ptree& available_devices)
{
  boost::property_tree::ptree empty_ptree;
  output << boost::format("  %-20s : %s\n") % "Version" % pt_xrt.get<std::string>("version", "N/A");
  output << boost::format("  %-20s : %s\n") % "Branch" % pt_xrt.get<std::string>("branch", "N/A");
  output << boost::format("  %-20s : %s\n") % "Hash" % pt_xrt.get<std::string>("hash", "N/A");
  output << boost::format("  %-20s : %s\n") % "Hash Date" % pt_xrt.get<std::string>("build_date", "N/A");
  const boost::property_tree::ptree& available_drivers = pt_xrt.get_child("drivers", empty_ptree);
  for(auto& drv : available_drivers) {
    const boost::property_tree::ptree& driver = drv.second;
    std::string drv_name = driver.get<std::string>("name", "N/A");
    std::string drv_hash = driver.get<std::string>("hash", "N/A");
    if (!boost::iequals(drv_hash, "N/A")) {
      output << boost::format("  %-20s : %s, %s\n") % drv_name
          % driver.get<std::string>("version", "N/A") % driver.get<std::string>("hash", "N/A");
    } else {
      std::string drv_version = boost::iequals(drv_name, "N/A") ? drv_name : drv_name.append(" Version");
      output << boost::format("  %-20s : %s\n") % drv_version % driver.get<std::string>("version", "N/A");
    }
  }

  try {
    if (!available_devices.empty()) {
       const boost::property_tree::ptree& dev = available_devices.begin()->second;
       if (dev.get<std::string>("device_class") == xrt_core::query::device_class::enum_to_str(xrt_core::query::device_class::type::ryzen))
         output << boost::format("  %-20s : %s\n") % "NPU Firmware Version" % available_devices.begin()->second.get<std::string>("firmware_version");
       else
         output << boost::format("  %-20s : %s\n") % "Firmware Version" % available_devices.begin()->second.get<std::string>("firmware_version");
    }
  }
  catch (...) {
    //no device available
  }
}// end of namespace XBValidateUtils
