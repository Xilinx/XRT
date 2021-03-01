/**
 * Copyright (C) 2020 Xilinx, Inc
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

// Utilities
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/format.hpp>

static boost::format fmtBasic("  %-20s : %s\n");

void 
ReportPlatform::getPropertyTreeInternal( const xrt_core::device * device,
                                         boost::property_tree::ptree &pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(device, pt);
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
  return ((sc.empty()) || (installed.bmcVer == sc));
}

/*
 * scan for plps installed on the system
 */
static boost::property_tree::ptree
get_installed_partitions(std::string interface_uuid)
{
  auto availableDSAs = firmwareImage::getIntalledDSAs();
  boost::property_tree::ptree pt_plps;
  for(unsigned int i = 0; i < availableDSAs.size(); i++) {
    boost::property_tree::ptree pt_plp;
    DSAInfo installedDSA = availableDSAs[i];
    if(installedDSA.hasFlashImage || installedDSA.uuids.empty())
      continue;
    pt_plp.put("vbnv", installedDSA.name);
    //the first UUID is always the logic UUID
    pt_plp.put("logic-uuid", XBU::string_to_UUID(installedDSA.uuids[0]));

    // Find the UUID that it exposes for other partitions
    for(unsigned int j = 1; j < installedDSA.uuids.size(); j++){
      //check if the interface UUID is resolution of BLP
      if(interface_uuid.compare(installedDSA.uuids[j]) != 0)
        continue;
      pt_plp.put("interface-uuid", XBU::string_to_UUID(installedDSA.uuids[j]));
    }
    pt_plp.put("file", installedDSA.file);

    //if the partition doesn't resolve the passed in BLP, don't add it to the list
    if(!pt_plp.get<std::string>("interface-uuid", "").empty())
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
  pt_platform.put("flash_type", xrt_core::device_query<xrt_core::query::flash_type>(device)); 
  pt_platform.put("hardware.serial_num", info.mSerialNum);

  //Flashable partition running on FPGA

  std::vector<std::string> logic_uuids, interface_uuids;
  // the vectors are being populated by empty strings which need to be removed
  try {
    logic_uuids = xrt_core::device_query<xrt_core::query::logic_uuids>(device);
    logic_uuids.erase(
      std::remove_if(logic_uuids.begin(), logic_uuids.end(),	
                      [](const std::string& s) { return s.empty(); }), logic_uuids.end());
  } catch (...) {}
  try {
    interface_uuids = xrt_core::device_query<xrt_core::query::interface_uuids>(device);
    interface_uuids.erase(
      std::remove_if(interface_uuids.begin(), interface_uuids.end(),	
                  [](const std::string& s) { return s.empty(); }), interface_uuids.end());
  } catch (...) {}
  
  
  boost::property_tree::ptree pt_current_shell;
  if(xrt_core::device_query<xrt_core::query::is_mfg>(device)) { // golden
    std::string board_name = xrt_core::device_query<xrt_core::query::board_name>(device);
    std::string vbnv = "xilinx_" + board_name + "_GOLDEN";
    pt_current_shell.put("vbnv", vbnv);
  } else if(!logic_uuids.empty() && !interface_uuids.empty()) { // 2RP
      DSAInfo part("", NULL_TIMESTAMP, logic_uuids[0], ""); 
      pt_current_shell.put("vbnv", part.name);
      pt_current_shell.put("logic-uuid", XBU::string_to_UUID(logic_uuids[0]));
      pt_current_shell.put("interface-uuid", XBU::string_to_UUID(interface_uuids[0]));
      pt_current_shell.put("id", (boost::format("0x%x") % part.timestamp));

      boost::property_tree::ptree pt_plps;
      for(unsigned int i = 1; i < logic_uuids.size(); i++) {
        boost::property_tree::ptree pt_plp;
        DSAInfo partition("", NULL_TIMESTAMP, logic_uuids[i], ""); 
        pt_plp.put("vbnv", partition.name);
        pt_plp.put("logic-uuid", XBU::string_to_UUID(logic_uuids[i]));
        pt_plp.put("interface-uuid", XBU::string_to_UUID(interface_uuids[i]));
        pt_plps.push_back( std::make_pair("", pt_plp) );
      }
      pt_platform.put_child("current_partitions", pt_plps);
  } else { //1RP
    pt_current_shell.put("vbnv", xrt_core::device_query<xrt_core::query::rom_vbnv>(device));
    pt_current_shell.put("id", (boost::format("0x%x") % xrt_core::device_query<xrt_core::query::rom_time_since_epoch>(device)));
  }

  std::string sc_ver;
  try {
    sc_ver = xrt_core::device_query<xrt_core::query::xmc_sc_version>(device);
  } catch (...) {}
  if(sc_ver.empty())
    sc_ver = info.mBMCVer;
  pt_current_shell.put("sc_version", sc_ver);
  pt_platform.add_child("current_shell", pt_current_shell);

  //Flashable partitions installed in system
  std::vector<DSAInfo> availableDSAs = f.getInstalledDSA();
  boost::property_tree::ptree pt_available_shells;
  for(unsigned int i = 0; i < availableDSAs.size(); i++) {
    boost::property_tree::ptree pt_available_shell;
    DSAInfo installedDSA = availableDSAs[i];
    pt_available_shell.put("vbnv", installedDSA.name);
    pt_available_shell.put("sc_version", installedDSA.bmcVer);
    pt_available_shell.put("id", (boost::format("0x%x") % installedDSA.timestamp));
    pt_available_shell.put("file", installedDSA.file);

    boost::property_tree::ptree pt_status;
    pt_status.put("shell", same_shell( pt_current_shell.get<std::string>("vbnv", ""), 
              pt_current_shell.get<std::string>("id", ""), installedDSA));
    pt_status.put("sc", same_sc( pt_current_shell.get<std::string>("sc_version", ""), installedDSA));
    pt_platform.add_child("status", pt_status);

    pt_available_shells.push_back( std::make_pair("", pt_available_shell) );
  }
  pt_platform.put_child("available_shells", pt_available_shells);

  if (!interface_uuids.empty()) {
    auto pt_available_partitions = get_installed_partitions(interface_uuids[0]);
    pt_platform.put_child("available_partitions", pt_available_partitions);
  }
  

  // There can only be 1 root node
  pt.add_child("platform", pt_platform);
}

static const std::string
shell_status(bool shell_status, bool sc_status, int num_dsa_shells)
{
  if(num_dsa_shells == 0)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "No shell is installed on the system.");
  if(num_dsa_shells > 1)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "Multiple shells are installed on the system.");
  if(!shell_status)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "Device is not up-to-date.");
  if(!sc_status)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "SC image on the device is not up-to-date.");
  return "";
}

void 
ReportPlatform::writeReport( const xrt_core::device * device, 
                             const std::vector<std::string> & /*_elementsFilter*/, 
                             std::iostream & output) const
{
  boost::property_tree::ptree pt;
  getPropertyTreeInternal(device, pt);

  output << "Flash properties\n";
  output << fmtBasic % "Type" % pt.get<std::string>("platform.flash_type", "N/A");
  output << fmtBasic % "Serial Number" % pt.get<std::string>("platform.hardware.serial_num", "N/A");
  output << std::endl;

  output << "Flashable partitions running on FPGA\n";
  output << fmtBasic % "Platform" % pt.get<std::string>("platform.current_shell.vbnv", "N/A");
  output << fmtBasic % "SC Version" % pt.get<std::string>("platform.current_shell.sc_version", "N/A");

  // print platform ID, for 2RP, platform ID = logic UUID 
  auto logic_uuid = pt.get<std::string>("platform.current_shell.logic-uuid", "");
  auto interface_uuid = pt.get<std::string>("platform.current_shell.interface-uuid", "");
  if (!logic_uuid.empty() && !interface_uuid.empty()) {
    output << fmtBasic % "Platform ID" % logic_uuid;
    output << fmtBasic % "Interface UUID" % interface_uuid;
  } else {
    output << fmtBasic % "Platform ID" % pt.get<std::string>("platform.current_shell.id", "N/A");
  }
  output << std::endl;

  // List PLP running on the system
  boost::property_tree::ptree pt_empty;
  boost::property_tree::ptree& plps = pt.get_child("platform.current_partitions", pt_empty);
  for(auto& kv : plps) {
    boost::property_tree::ptree& plp = kv.second;
    output << fmtBasic % "Platform" % plp.get<std::string>("vbnv", "N/A");
    output << fmtBasic % "Logic UUID" % plp.get<std::string>("logic-uuid", "N/A");
    output << fmtBasic % "Interface UUID" % plp.get<std::string>("interface-uuid", "N/A");
    output << std::endl;
  }

  output << "Flashable partitions installed in system\n"; 
  boost::property_tree::ptree& available_shells = pt.get_child("platform.available_shells");
  
  if(available_shells.empty())
    output << boost::format("  %-20s\n") % "None" << std::endl;
  
  for(auto& kv : available_shells) {
    boost::property_tree::ptree& available_shell = kv.second;
    output << fmtBasic % "Platform" % available_shell.get<std::string>("vbnv", "N/A");
    output << fmtBasic % "SC Version" % available_shell.get<std::string>("sc_version", "N/A");
    output << fmtBasic % "Platform ID" % available_shell.get<std::string>("id", "N/A");
    output << std::endl;
  }

  //PLPs installed on the system
  boost::property_tree::ptree& available_plps = pt.get_child("platform.available_partitions", pt_empty);
  for(auto& kv : available_plps) {
    boost::property_tree::ptree& plp = kv.second;
    output << fmtBasic % "Platform" % plp.get<std::string>("vbnv", "N/A");
    output << fmtBasic % "Logic UUID" % plp.get<std::string>("logic-uuid", "N/A");
    output << fmtBasic % "Interface UUID" % plp.get<std::string>("interface-uuid", "N/A");
    output << std::endl;
  }

  output << "----------------------------------------------------\n"
         << shell_status(pt.get<bool>("platform.status.shell", false), 
                         pt.get<bool>("platform.status.sc", false),  static_cast<int>(available_shells.size()));
}
