/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportPlatform.h"
#include "flash/flasher.h"
#include "flash/firmware_image.h"
#include "core/common/query_requests.h"
#include "core/common/utils.h"
#include "core/common/info_vmr.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

static boost::format fmtBasic("  %-20s : %s\n");
static boost::format fmtBasicHex("  %-20s : 0x%x\n");

void
ReportPlatform::getPropertyTreeInternal( const xrt_core::device * device,
                                         boost::property_tree::ptree &pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(device, pt);
}

/**
 * New flow for exposing mac addresses
 * qr::mac_contiguous_num is the total number of mac addresses
 * avaliable contiguously starting from qr::mac_addr_first
 *
 * Old flow: Query the four sysfs nodes we have and validate them
 * before adding them to the property tree
 */
static boost::property_tree::ptree
mac_addresses(const xrt_core::device * dev)
{
  boost::property_tree::ptree ptree;
  uint64_t mac_contiguous_num = 0;
  std::string mac_addr_first;
  try {
    mac_contiguous_num = xrt_core::device_query<xrt_core::query::mac_contiguous_num>(dev);
    mac_addr_first = xrt_core::device_query<xrt_core::query::mac_addr_first>(dev);
  }
  catch (const xrt_core::query::exception&) {
  }

  //new flow
  if (mac_contiguous_num && !mac_addr_first.empty()) {
    // finding the contiguous mac address
    // 00:00:00:00:00:01
    // mac_prefix = 00:00:00:00:00
    // mac_base = 01 (gets increased by 1)
    // mac_base_val = 00:00:00:00:00:02
    std::string mac_prefix = mac_addr_first.substr(0, mac_addr_first.find_last_of(":"));
    std::string mac_base = mac_addr_first.substr(mac_addr_first.find_last_of(":") + 1);
    std::stringstream ss;
    uint32_t mac_base_val = 0;
    ss << std::hex << mac_base;
    ss >> mac_base_val;

    for (uint32_t i = 0; i < (uint32_t)mac_contiguous_num; i++) {
      boost::property_tree::ptree addr;
      auto base = boost::format("%02X") % (mac_base_val + i);
      addr.add("address", mac_prefix + ":" + base.str());
      ptree.push_back(std::make_pair("", addr));
    }
  }
  else { //old flow
    std::vector<std::string> mac_addr;
    try {
      mac_addr = xrt_core::device_query<xrt_core::query::mac_addr_list>(dev);
    }
    catch (const xrt_core::query::exception&) {
    }
    for (const auto& a : mac_addr) {
      boost::property_tree::ptree addr;
      if (!a.empty() && a.compare("FF:FF:FF:FF:FF:FF") != 0) {
        addr.add("address", a);
        ptree.push_back(std::make_pair("", addr));
      }
    }
  }

  return ptree;
}

static boost::property_tree::ptree
get_boot_info(const xrt_core::device * dev)
{
  boost::property_tree::ptree ptree;
  // get boot on default from vmr_status sysfs node
  boost::property_tree::ptree pt_empty;
  const auto pt = xrt_core::vmr::vmr_info(dev).get_child("vmr", pt_empty);
  for (auto& ks : pt) {
    const boost::property_tree::ptree& vmr_stat = ks.second;
    if (boost::iequals(vmr_stat.get<std::string>("label"), "Boot on default")) {
      auto is_default_boot = std::stoi(vmr_stat.get<std::string>("value"));
      ptree.add("default", is_default_boot ? "ACTIVE" : "INACTIVE");
      ptree.add("backup", is_default_boot ? "INACTIVE" : "ACTIVE");
      break;
    }
  }
  return ptree;
}

/*
 * helper function for getPropertyTree20202()
 */
static bool
same_shell(const std::string& vbnv, const std::string& id,
            const DSAInfo& installed)
{
  if (!vbnv.empty()) {
    bool same_dsa = ((installed.name == vbnv) &&
      (installed.matchId(id)));
    return same_dsa;
  }
  return false;
}

/*
 * helper function for getPropertyTree20202()
 */
static bool
same_sc(const std::string& sc, const DSAInfo& installed)
{
  return ((sc.empty()) || (installed.bmcVer.empty()) ||
          (installed.bmcVer == sc) || (sc.find("FIXED") != std::string::npos));
}

/*
 * scan for plps installed on the system
 */
static boost::property_tree::ptree
get_installed_partitions(std::string interface_uuid)
{
  auto availableDSAs = firmwareImage::getIntalledDSAs();
  boost::property_tree::ptree pt_plps;
  for (unsigned int i = 0; i < availableDSAs.size(); i++) {
    boost::property_tree::ptree pt_plp;
    DSAInfo installedDSA = availableDSAs[i];
    if (installedDSA.hasFlashImage || installedDSA.uuids.empty())
      continue;
    pt_plp.put("vbnv", installedDSA.name);

    //the first UUID is always the logic UUID
    std::string luuid = installedDSA.uuids.empty()
      ? ""
      : xrt_core::query::interface_uuids::to_uuid_upper_string(installedDSA.uuids[0]);
    pt_plp.put("logic-uuid", luuid);

    // Find the UUID that it exposes for other partitions
    for (unsigned int j = 1; j < installedDSA.uuids.size(); j++){
      //check if the interface UUID is resolution of BLP
      if (interface_uuid.compare(installedDSA.uuids[j]) != 0)
        continue;
      pt_plp.put("interface-uuid", xrt_core::query::interface_uuids::to_uuid_upper_string(installedDSA.uuids[j]));
    }
    pt_plp.put("file", installedDSA.file);

    //if the partition doesn't resolve the passed in BLP, don't add it to the list
    if (!pt_plp.get<std::string>("interface-uuid", "").empty())
      pt_plps.push_back( std::make_pair("", pt_plp) );
  }
  return pt_plps;
}

void
ReportPlatform::getPropertyTree20202( const xrt_core::device * device,
                                      boost::property_tree::ptree &pt) const
{
  boost::property_tree::ptree pt_platform;

  Flasher f(device->get_device_id());

  BoardInfo info;
  f.getBoardInfo(info);
  //create information tree for a device
  pt_platform.put("bdf", xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device)));
  pt_platform.put("flash_type", xrt_core::device_query<xrt_core::query::flash_type>(device));
  pt_platform.put("hardware.serial_num", info.mSerialNum.empty() ? "N/A" : info.mSerialNum);
  boost::property_tree::ptree dev_prop;
  dev_prop.put("board_type", xrt_core::device_query<xrt_core::query::board_name>(device));
  dev_prop.put("board_name", info.mName.empty() ? "N/A" : info.mName);
  dev_prop.put("config_mode", info.mConfigMode);
  dev_prop.put("max_power_watts", info.mMaxPower);
  pt_platform.add_child("device_properties", dev_prop);

  //Flashable partition running on FPGA

  std::vector<std::string> logic_uuids, interface_uuids;
  // the vectors are being populated by empty strings which need to be removed
  try {
    logic_uuids = xrt_core::device_query<xrt_core::query::logic_uuids>(device);
    logic_uuids.erase(
      std::remove_if(logic_uuids.begin(), logic_uuids.end(),
                      [](const std::string& s) { return s.empty(); }), logic_uuids.end());
  } catch (const xrt_core::query::exception&) {
  }
  try {
    interface_uuids = xrt_core::device_query<xrt_core::query::interface_uuids>(device);
    interface_uuids.erase(
      std::remove_if(interface_uuids.begin(), interface_uuids.end(),
                  [](const std::string& s) { return s.empty(); }), interface_uuids.end());
  } catch (const xrt_core::query::exception&) {
  }

  boost::property_tree::ptree pt_current_shell;
  if (xrt_core::device_query<xrt_core::query::is_mfg>(device)) { // golden
    auto mGoldenVer = xrt_core::device_query<xrt_core::query::mfg_ver>(device);
    std::string board_name = xrt_core::device_query<xrt_core::query::board_name>(device);
    std::string vbnv = "xilinx_" + board_name + "_GOLDEN_" + std::to_string( mGoldenVer);
    pt_current_shell.put("vbnv", vbnv);
  } else if (!logic_uuids.empty() && !interface_uuids.empty()) { // 2RP
    DSAInfo part("", NULL_TIMESTAMP, logic_uuids[0], "");
    pt_current_shell.put("vbnv", (part.name).empty() ? xrt_core::device_query<xrt_core::query::rom_vbnv>(device) : part.name);
    pt_current_shell.put("logic-uuid", xrt_core::query::interface_uuids::to_uuid_upper_string(logic_uuids[0]));
    pt_current_shell.put("interface-uuid", xrt_core::query::interface_uuids::to_uuid_upper_string(interface_uuids[0]));
    pt_current_shell.put("id", (boost::format("0x%x") % part.timestamp));

    boost::property_tree::ptree pt_plps;
    for (unsigned int i = 1; i < logic_uuids.size(); i++) {
      boost::property_tree::ptree pt_plp;
      DSAInfo partition("", NULL_TIMESTAMP, logic_uuids[i], "");
      pt_plp.put("vbnv", partition.name);
      pt_plp.put("logic-uuid", xrt_core::query::interface_uuids::to_uuid_upper_string(logic_uuids[i]));
      pt_plp.put("interface-uuid", xrt_core::query::interface_uuids::to_uuid_upper_string(interface_uuids[i]));
      pt_plps.push_back( std::make_pair("", pt_plp) );
    }
    pt_platform.put_child("current_partitions", pt_plps);
  } else { //1RP
    pt_current_shell.put("vbnv", xrt_core::device_query<xrt_core::query::rom_vbnv>(device));
    pt_current_shell.put("id", (boost::format("0x%x") % xrt_core::device_query<xrt_core::query::rom_time_since_epoch>(device)));
  }

  std::string sc_ver = info.mBMCVer;
  if (sc_ver.empty()) {
    try {
      sc_ver = xrt_core::device_query<xrt_core::query::xmc_sc_version>(device);
    } catch (const xrt_core::query::exception&) {
      auto board = f.getOnBoardDSA();
      sc_ver = board.bmc_ver();
    }
  }

  pt_current_shell.put("sc_version", sc_ver);
  pt_platform.add_child("current_shell", pt_current_shell);

  //Flashable partitions installed in system
  std::vector<DSAInfo> availableDSAs = f.getInstalledDSA();
  boost::property_tree::ptree pt_available_shells;
  for (unsigned int i = 0; i < availableDSAs.size(); i++) {
    boost::property_tree::ptree pt_available_shell;
    DSAInfo installedDSA = availableDSAs[i];
    pt_available_shell.put("vbnv", installedDSA.name);
    pt_available_shell.put("sc_version", installedDSA.bmcVer);
    pt_available_shell.put("id", (boost::format("0x%x") % installedDSA.timestamp));
    //the first UUID is always the logic UUID
    std::string uuid = installedDSA.uuids.empty()
      ? ""
      : xrt_core::query::interface_uuids::to_uuid_upper_string(installedDSA.uuids[0]);
    pt_available_shell.put("logic-uuid", uuid);
    pt_available_shell.put("file", installedDSA.file);

    boost::property_tree::ptree pt_status;
    pt_status.put("shell", same_shell( pt_current_shell.get<std::string>("vbnv", ""), 
              pt_current_shell.get<std::string>("id", ""), installedDSA));
    pt_status.put("sc", same_sc( pt_current_shell.get<std::string>("sc_version", ""), installedDSA));
    pt_status.put("is_factory", xrt_core::device_query<xrt_core::query::is_mfg>(device));
    pt_status.put("is_recovery", xrt_core::device_query<xrt_core::query::is_recovery>(device));
    pt_platform.add_child("status", pt_status);

    pt_available_shells.push_back( std::make_pair("", pt_available_shell) );
  }
  pt_platform.put_child("available_shells", pt_available_shells);

  if (!interface_uuids.empty()) {
    auto pt_available_partitions = get_installed_partitions(interface_uuids[0]);
    pt_platform.put_child("available_partitions", pt_available_partitions);
  }

  auto macs = mac_addresses(device);
  if (!macs.empty())
    pt_platform.put_child("macs", macs);

  auto pt_boot = get_boot_info(device);
  if (!pt_boot.empty())
    pt_platform.put_child("bootable_partition", pt_boot);

  // There can only be 1 root node
  pt.add_child("platform", pt_platform);
}

static const std::string
shell_status(bool shell_status, bool sc_status, int num_dsa_shells)
{
  if (num_dsa_shells == 0)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "No shell is installed on the system.");

  if (num_dsa_shells > 1)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "Multiple shells are installed on the system.");

  if (!shell_status)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "Device is not up-to-date.");

  if (!sc_status)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "SC image on the device is not up-to-date.");

  return "";
}

void
ReportPlatform::writeReport( const xrt_core::device* /*_pDevice*/,
                             const boost::property_tree::ptree& _pt,
                             const std::vector<std::string>& /*_elementsFilter*/,
                             std::ostream & _output) const
{
  auto string_or_NA =[](std::string str) {
    return str.empty() ? "N/A" : str;
  };

  _output << "Flash properties\n";
  _output << fmtBasic % "Type" % string_or_NA(_pt.get<std::string>("platform.flash_type"));
  _output << fmtBasic % "Serial Number" % string_or_NA(_pt.get<std::string>("platform.hardware.serial_num"));
  _output << std::endl;

  const boost::property_tree::ptree& dev_properties = _pt.get_child("platform.device_properties");
  _output << "Device properties\n";
  _output << fmtBasic % "Type" % string_or_NA(dev_properties.get<std::string>("board_type"));
  _output << fmtBasic % "Name" % string_or_NA(dev_properties.get<std::string>("board_name"));
  auto config_mode = dev_properties.get<unsigned int>("config_mode");
  if (config_mode != 0)
    _output << fmtBasicHex % "Config Mode" % config_mode;
  auto max_power_str = dev_properties.get<std::string>("max_power_watts");
  if (!max_power_str.empty())
    _output << fmtBasic % "Max Power" % max_power_str;
  _output << std::endl;

  _output << "Flashable partitions running on FPGA\n";
  _output << fmtBasic % "Platform" % string_or_NA(_pt.get<std::string>("platform.current_shell.vbnv"));
  _output << fmtBasic % "SC Version" % string_or_NA(_pt.get<std::string>("platform.current_shell.sc_version"));

  // print platform ID, for 2RP, platform ID = logic UUID
  auto logic_uuid = _pt.get<std::string>("platform.current_shell.logic-uuid", "");
  auto interface_uuid = _pt.get<std::string>("platform.current_shell.interface-uuid", "");
  if (!logic_uuid.empty() && !interface_uuid.empty()) {
    _output << fmtBasic % "Platform UUID" % logic_uuid;
    _output << fmtBasic % "Interface UUID" % interface_uuid;
  } else {
    _output << fmtBasic % "Platform ID" % string_or_NA(_pt.get<std::string>("platform.current_shell.id", ""));
  }
  _output << std::endl;

  // List PLP running on the system
  boost::property_tree::ptree pt_empty;

  const boost::property_tree::ptree& plps = _pt.get_child("platform.current_partitions", pt_empty);
  for (auto& kv : plps) {
    const boost::property_tree::ptree& plp = kv.second;
    _output << fmtBasic % "Platform" % string_or_NA(plp.get<std::string>("vbnv"));
    _output << fmtBasic % "Logic UUID" % string_or_NA(plp.get<std::string>("logic-uuid"));
    _output << fmtBasic % "Interface UUID" % string_or_NA(plp.get<std::string>("interface-uuid"));
    _output << std::endl;
  }

  _output << "Flashable partitions installed in system\n";
  const boost::property_tree::ptree& available_shells = _pt.get_child("platform.available_shells");

  if (available_shells.empty())
    _output << boost::format("  %-20s\n") % "<none found>" << std::endl;

  for (auto& kv : available_shells) {
    const boost::property_tree::ptree& available_shell = kv.second;
    _output << fmtBasic % "Platform" % string_or_NA(available_shell.get<std::string>("vbnv"));
    _output << fmtBasic % "SC Version" % string_or_NA(available_shell.get<std::string>("sc_version"));
    // print platform ID, for 2RP, platform ID = logic UUID
    auto platform_uuid = available_shell.get<std::string>("logic-uuid", "");
    if (!platform_uuid.empty()) {
      _output << fmtBasic % "Platform UUID" % platform_uuid;
    } else {
      _output << fmtBasic % "Platform ID" % string_or_NA(available_shell.get<std::string>("id"));
    }
      _output << std::endl;
  }

  const auto& partition = _pt.get_child("platform.bootable_partition", pt_empty);
  if (!partition.empty()) {
    _output << "Bootable Partitions:" << std::endl;
    _output << fmtBasic % "Default" % partition.get<std::string>("default");
    _output << fmtBasic % "Backup" % partition.get<std::string>("backup");
    _output << std::endl;
  }

  //PLPs installed on the system
  const boost::property_tree::ptree& available_plps = _pt.get_child("platform.available_partitions", pt_empty);
  for (auto& kv : available_plps) {
    const boost::property_tree::ptree& plp = kv.second;
    _output << fmtBasic % "Platform" % string_or_NA(plp.get<std::string>("vbnv"));
    _output << fmtBasic % "Logic UUID" % string_or_NA(plp.get<std::string>("logic-uuid"));
    _output << fmtBasic % "Interface UUID" % string_or_NA(plp.get<std::string>("interface-uuid"));
    _output << std::endl;
  }

  const boost::property_tree::ptree& macs = _pt.get_child("platform.macs", pt_empty);
  if (!macs.empty()) {
    _output << std::endl;
    std::string formattedStr;

    for (auto & km : macs)
      formattedStr += boost::str(fmtBasic % (formattedStr.empty() ? "Mac Address" : "") % string_or_NA(km.second.get<std::string>("address")));
    _output << formattedStr << std::endl;
  }

  _output << shell_status(_pt.get<bool>("platform.status.shell", false),
                          _pt.get<bool>("platform.status.sc", false),
                          static_cast<int>(available_shells.size()));
  _output << std::endl;
}
