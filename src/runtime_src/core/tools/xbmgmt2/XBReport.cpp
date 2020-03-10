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
#include "XBReport.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;
#include "common/system.h"
#include "common/device.h"
#include "flash/flasher.h"
#include "core/common/error.h"
#include "core/common/query_requests.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/property_tree/json_parser.hpp>

// System - Include Files

// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBReport;


// ------ F U N C T I O N S ---------------------------------------------------

void
XBReport::report_thermal_devices(const std::vector<uint16_t>& device_indices, boost::property_tree::ptree& pt, bool json)
{
  for(auto& idx : device_indices) {
    //collect information from various ptree interfaces
    boost::property_tree::ptree thermal_fpga;
    boost::property_tree::ptree on_board_dev_info;
    auto device = xrt_core::get_mgmtpf_device(idx);
    device->read_thermal_fpga(thermal_fpga);
    device->get_info(on_board_dev_info);

    // Create our array of data
    pt.push_back(std::make_pair(on_board_dev_info.get<std::string>("bdf"), thermal_fpga));

    if(!json) {
      XBU::message(boost::str(boost::format("%s: %s") % "BDF" %  on_board_dev_info.get<std::string>("bdf")));
      XBU::message("\nTemperature");
      XBU::message(boost::str(boost::format("  %-18s: %s C") % "Temp" %  thermal_fpga.get<std::string>("temp_C")));
      XBU::message("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    }
  }
  if(json) {
    std::ostringstream buf;
    boost::property_tree::write_json(buf, pt, true);
    std::cout << buf.str();
  }
}

void
XBReport::report_electrical_devices(const std::vector<uint16_t>& device_indices, boost::property_tree::ptree& pt, bool json)
{
  for(auto& idx : device_indices) {
    //collect information from various ptree interfaces
    boost::property_tree::ptree electrical;
    boost::property_tree::ptree on_board_dev_info;
    auto device = xrt_core::get_mgmtpf_device(idx);
    device->read_electrical(electrical);
    device->get_info(on_board_dev_info);

    // Create our array of data
    pt.push_back(std::make_pair(on_board_dev_info.get<std::string>("bdf"), electrical));

    if(!json) {
      XBU::message(boost::str(boost::format("%s: %s") % "BDF" %  on_board_dev_info.get<std::string>("bdf")));
      XBU::message("\nElectrical");
      XBU::message("  To-Do");
      XBU::message("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    }
  }
  if(json) {
    std::ostringstream buf;
    boost::property_tree::write_json(buf, pt, true);
    std::cout << buf.str();
  }
}

void
XBReport::report_fans_devices(const std::vector<uint16_t>& device_indices, boost::property_tree::ptree& pt, bool json)
{
  for(auto& idx : device_indices) {
    //collect information from various ptree interfaces
    boost::property_tree::ptree fan_info;
    boost::property_tree::ptree on_board_dev_info;
    auto device = xrt_core::get_mgmtpf_device(idx);
    device->read_fan_info(fan_info);
    device->get_info(on_board_dev_info);

    // Create our array of data
    pt.push_back(std::make_pair(on_board_dev_info.get<std::string>("bdf"), fan_info));

    if(!json) {
      XBU::message(boost::str(boost::format("%s: %s") % "BDF" %  on_board_dev_info.get<std::string>("bdf")));
      XBU::message("\nFans");
      XBU::message(boost::str(boost::format("  %-22s: %s C") % "Temp trigger critical" %  fan_info.get<std::string>("temp_trigger_critical_C")));
      XBU::message(boost::str(boost::format("  %-22s: %s") % "Fan presence" %  fan_info.get<std::string>("fan_presence")));
      XBU::message(boost::str(boost::format("  %-22s: %s rpm") % "Fan speed" %  fan_info.get<std::string>("fan_speed_rpm")));
      XBU::message("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    }
  }
  if(json) {
    std::ostringstream buf;
    boost::property_tree::write_json(buf, pt, true);
    std::cout << buf.str();
  }
}

/*
 * helper function for report_shell_on_devices()
 */
static bool 
same_config(const std::string& vbnv, const std::string& sc,
             const std::string& id, DSAInfo& installed) 
{
  if (!vbnv.empty()) {
    bool same_dsa = ((installed.name == vbnv) &&
      (installed.matchId(id)));
    bool same_bmc = ((sc.empty()) ||
      (installed.bmcVer == sc));
    return same_dsa && same_bmc;
  }
  return false;
}

static const std::string
shell_status(bool status)
{
  if(status)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "Device is not up-to-date.");
  return "";
}

void
XBReport::report_shell_on_devices(const std::vector<uint16_t>& device_indices, boost::property_tree::ptree& pt, bool json)
{
  for(auto& idx : device_indices) {
    //collect information from various ptree interfaces
    boost::property_tree::ptree pt_device;
    boost::property_tree::ptree on_board_rom_info;
    boost::property_tree::ptree on_board_dev_info;
    boost::property_tree::ptree on_board_platform_info;
    boost::property_tree::ptree on_board_xmc_info;
    auto device = xrt_core::get_mgmtpf_device(idx);
    device->get_rom_info(on_board_rom_info);
    device->get_info(on_board_dev_info);
    device->get_platform_info(on_board_platform_info);
    device->get_xmc_info(on_board_xmc_info);

    Flasher f(idx);
    if(!f.isValid()) {
      xrt_core::error(boost::str(boost::format("%d is an invalid index") % idx));
      continue;
    }
    std::vector<DSAInfo> installedDSA = f.getInstalledDSA();

    BoardInfo info;
    f.getBoardInfo(info);

    //create information tree for a device
    pt_device.put("flash_type", on_board_platform_info.get<std::string>("flash_type", "N/A"));
    //Flashable partition running on FPGA
    pt_device.put("shell_on_fpga.vbnv", on_board_rom_info.get<std::string>("vbnv", "N/A"));
    pt_device.put("shell_on_fpga.sc_version", on_board_xmc_info.get<std::string>("sc_version", "N/A"));
    pt_device.put("shell_on_fpga.id", on_board_rom_info.get<std::string>("id", "N/A"));

    //Flashable partitions installed in system
    pt_device.put("installed_shell.vbnv", installedDSA.front().name);
    pt_device.put("installed_shell.sc_version", installedDSA.front().bmcVer);
    pt_device.put("installed_shell.id", (boost::format("0x%x") % installedDSA.front().timestamp));
    pt_device.put("shell_upto_date", false);

    //check if the platforms on the machine and card match
    if(!same_config( on_board_rom_info.get<std::string>("vbnv"), on_board_xmc_info.get<std::string>("sc_version"),
        on_board_rom_info.get<std::string>("id"), installedDSA.front())) {
      pt_device.put("shell_upto_date", true);
    }

    // Create our array of data
    pt.push_back(std::make_pair(on_board_dev_info.get<std::string>("bdf"), pt_device));

    if(!json) {
        std::cout << boost::format("%s : %d\n") % "BDF" % on_board_dev_info.get<std::string>("bdf");
        std::cout << boost::format("  %-20s : %s\n") % "Flash type" % pt_device.get<std::string>("flash_type", "N/A");

        std::cout << "Flashable partition running on FPGA\n";
        std::cout << boost::format("  %-20s : %s\n") % "Platform" % pt_device.get<std::string>("shell_on_fpga.vbnv", "N/A");
        std::cout << boost::format("  %-20s : %s\n") % "SC Version" % pt_device.get<std::string>("shell_on_fpga.sc_version", "N/A");
        std::cout << boost::format("  %-20s : 0x%x\n") % "Platform ID" % pt_device.get<std::string>("shell_on_fpga.id", "N/A");

        std::cout << "\nFlashable partitions installed in system\n";
        std::cout << boost::format("  %-20s : %s\n") % "Platform" % pt_device.get<std::string>("installed_shell.vbnv", "N/A");
        std::cout << boost::format("  %-20s : %s\n") % "SC Version" % pt_device.get<std::string>("installed_shell.sc_version", "N/A");
        std::cout << boost::format("  %-20s : 0x%x\n") % "Platform ID" % pt_device.get<std::string>("installed_shell.id", "N/A");
        std::cout << shell_status(pt_device.get<bool>("shell_upto_date"));
        std::cout << "----------------------------------------------------\n";
    }
  }
  if(json) {
    std::ostringstream buf;
    boost::property_tree::write_json(buf, pt, true);
    std::cout << buf.str();
  }
}