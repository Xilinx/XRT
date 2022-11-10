// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
#include "XBUtilities.h"
#include "XBUtilitiesCore.h"

// Local - Include Files
#include "core/common/error.h"
#include "core/common/info_vmr.h"
#include "core/common/utils.h"
#include "core/common/message.h"

#include "common/system.h"

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
      pt_dev.put("vbnv", xrt_core::device_query<xrt_core::query::rom_vbnv>(device));
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
      auto instance = xrt_core::device_query<xrt_core::query::instance>(device);
      std::string pf = device->is_userpf() ? "user" : "mgmt";
      pt_dev.put("instance",boost::str(boost::format("%s(inst=%d)") % pf % instance));
    }
    catch(const xrt_core::query::exception&) {
        // The instance wasn't added
    }

    }
    pt_dev.put("is_ready", xrt_core::device_query<xrt_core::query::is_ready>(device));
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
str_available_devs(bool _inUserDomain)
{
  //gather available devices for user to pick from
  std::stringstream available_devs;
  available_devs << "\n Available devices:\n";
  boost::property_tree::ptree available_devices = XBUtilities::get_available_devices(_inUserDomain);
  for (auto& kd : available_devices) {
    boost::property_tree::ptree& dev = kd.second;
    available_devs << boost::format("  [%s] : %s\n") % dev.get<std::string>("bdf") % dev.get<std::string>("vbnv");
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
    throw std::runtime_error("Invalid BDF format. Please specify valid BDF" + str_available_devs(_inUserDomain));

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
    throw std::runtime_error(boost::str(boost::format("Invalid BDF '%s'. Please spcify the BDF using 'DDDD:BB:DD.F' format") % bdfstr) + str_available_devs(_inUserDomain));

  std::reverse(std::begin(tokens), std::end(tokens));

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

  throw std::runtime_error(boost::str(boost::format("Specified device BDF '%s' not found") % bdfstr) + str_available_devs(_inUserDomain));
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
    auto device = _inUserDomain ? xrt_core::get_userpf_device(idx) : xrt_core::get_mgmtpf_device(idx);

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
  xrt_core::get_xrt_info(pt_xrt);
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
        if(_inUserDomain)
          _deviceCollection.push_back( xrt_core::get_userpf_device(index) );
        else
          _deviceCollection.push_back( xrt_core::get_mgmtpf_device(index) );
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
    if(_inUserDomain)
      _deviceCollection.push_back( xrt_core::get_userpf_device(index) );
    else
      _deviceCollection.push_back( xrt_core::get_mgmtpf_device(index) );
  }
}

  static void 
  check_versal_boot(const std::shared_ptr<xrt_core::device> &device)
  {
    if (xrt_core::vmr::is_default_boot(device.get()))
      return;

    std::cout << "***********************************************************\n";
    std::cout << "*        WARNING          WARNING          WARNING        *\n";
    std::cout << "*             Versal Platform in backup boot              *\n";
    std::cout << "***********************************************************\n";
  }

  std::shared_ptr<xrt_core::device>
  XBUtilities::get_device( const std::string &deviceBDF, bool in_user_domain)
  {
    // -- If the deviceBDF is empty then do nothing
    if (deviceBDF.empty())
      throw std::runtime_error("Please specify a device using --device option" + str_available_devs(in_user_domain));

    // -- Collect the devices by name
    auto index = str2index(deviceBDF, in_user_domain);    // Can throw
    std::shared_ptr<xrt_core::device> device;
    if(in_user_domain)
      device = xrt_core::get_userpf_device(index);
    else
      device = xrt_core::get_mgmtpf_device(index);

    if (xrt_core::device_query<xq::is_versal>(device))
      check_versal_boot(device);

    return device;
  }

void
XBUtilities::can_proceed_or_throw(const std::string& info, const std::string& error)
{
  std::cout << info << "\n";
  if (!XBUtilities::can_proceed(getForce()))
    throw xrt_core::system_error(ECANCELED, error);
}

void
XBUtilities::sudo_or_throw(const std::string& msg)
{
#ifndef _WIN32
  if ((getuid() == 0) || (geteuid() == 0))
    return;

  std::cerr << "ERROR: " << msg << std::endl;
  throw xrt_core::error(std::errc::operation_canceled);
#endif
}

void
XBUtilities::throw_cancel(const std::string& msg)
{
  throw_cancel(boost::format("%s") % msg);
}

void
XBUtilities::throw_cancel(const boost::format& format)
{
  throw xrt_core::error(std::errc::operation_canceled, boost::str(format));
}

void
XBUtilities::print_exception(const std::system_error& e)
{
  // Remove the type of error from the message.
  const std::string msg = std::regex_replace(e.what(), std::regex(std::string(": ") + e.code().message()), "");

  if (!msg.empty())
    std::cerr << boost::format("ERROR: %s\n") % msg;
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
  xrt_core::get_xrt_info(pt_xrt);
  boost::property_tree::ptree empty_ptree;

  ss << boost::format("%-20s : %s\n") % "Version" % pt_xrt.get<std::string>("version", "N/A");
  ss << boost::format("%-20s : %s\n") % "Branch" % pt_xrt.get<std::string>("branch", "N/A");
  ss << boost::format("%-20s : %s\n") % "Hash" % pt_xrt.get<std::string>("hash", "N/A");
  ss << boost::format("%-20s : %s\n") % "Hash Date" % pt_xrt.get<std::string>("build_date", "N/A");
  const boost::property_tree::ptree& available_drivers = pt_xrt.get_child("drivers", empty_ptree);
  for(auto& drv : available_drivers) {
    const boost::property_tree::ptree& driver = drv.second;
    std::string drv_name = driver.get<std::string>("name", "N/A");
    boost::algorithm::to_upper(drv_name);
    ss << boost::format("%-20s : %s, %s\n") % drv_name
        % driver.get<std::string>("version", "N/A") % driver.get<std::string>("hash", "N/A");
  }
  return ss.str();
}
