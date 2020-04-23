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

// 3rd Party Library - Include Files
#include <boost/format.hpp>


void 
ReportPlatform::getPropertyTreeInternal( const xrt_core::device * _pDevice,
                                         boost::property_tree::ptree &_pt) const
{
  // Defer to the 20201 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20201(_pDevice, _pt);
}

/*
 * helper function for getPropertyTree20201()
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
 * helper function for getPropertyTree20201()
 */
static bool 
same_sc(const std::string& sc, const DSAInfo& installed) 
{
  return ((sc.empty()) || (installed.bmcVer == sc));
}

void 
ReportPlatform::getPropertyTree20201( const xrt_core::device * _pDevice,
                                      boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  pt.put("Description","Platform Information");

  // There can only be 1 root node
  _pt.add_child("platform", pt);

  boost::property_tree::ptree on_board_rom_info;
  boost::property_tree::ptree on_board_platform_info;
  boost::property_tree::ptree on_board_xmc_info;
  boost::property_tree::ptree on_board_dev_info;
  _pDevice->get_rom_info(on_board_rom_info);
  _pDevice->get_platform_info(on_board_platform_info);
  _pDevice->get_xmc_info(on_board_xmc_info);
  _pDevice->get_info(on_board_dev_info);

  //create information tree for a device
  _pt.put("platform.bdf", on_board_dev_info.get<std::string>("bdf"));
  _pt.put("platform.flash_type", on_board_platform_info.get<std::string>("flash_type", "N/A"));
  //Flashable partition running on FPGA
  _pt.put("platform.shell_on_fpga.vbnv", on_board_rom_info.get<std::string>("vbnv", "N/A"));
  _pt.put("platform.shell_on_fpga.sc_version", on_board_xmc_info.get<std::string>("sc_version", "N/A"));
  _pt.put("platform.shell_on_fpga.id", on_board_rom_info.get<std::string>("id", "N/A"));

  Flasher f(_pDevice->get_device_id());
  std::vector<DSAInfo> installedDSAs = f.getInstalledDSA();
  _pt.put("platform.number_of_installed_shells", installedDSAs.size());

  BoardInfo info;
  f.getBoardInfo(info);

  //Flashable partitions installed in system
  for(unsigned int i = 0; i < installedDSAs.size(); i++) {
    boost::property_tree::ptree _ptInstalledShell;
    DSAInfo installedDSA = installedDSAs[i];
    _ptInstalledShell.put("vbnv", installedDSA.name);
    _ptInstalledShell.put("sc_version", installedDSA.bmcVer);
    _ptInstalledShell.put("id", (boost::format("0x%x") % installedDSA.timestamp));
    _ptInstalledShell.put("file", installedDSA.file);
    _pt.put("platform.shell_upto_date", same_shell( on_board_rom_info.get<std::string>("vbnv", ""), 
              on_board_rom_info.get<std::string>("id", ""), installedDSA));
    _pt.put("platform.sc_upto_date", same_sc( on_board_xmc_info.get<std::string>("sc_version", ""), 
             installedDSA));
    _pt.add_child("platform.installed_shell." + std::to_string(i), _ptInstalledShell);
  }

}

static const std::string
shell_status(bool shell_status, bool sc_status, int multiDSA)
{
  if(multiDSA > 1)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "Multiple shells are installed on the system.");
  if(!shell_status)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "Device is not up-to-date.");
  if(!sc_status)
    return boost::str(boost::format("%-8s : %s\n") % "WARNING" % "SC image on the device is not up-to-date.");
  return "";
}

void 
ReportPlatform::writeReport( const xrt_core::device * _pDevice, 
                             const std::vector<std::string> & /*_elementsFilter*/, 
                             std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  getPropertyTreeInternal(_pDevice, _pt);

  _output << boost::format("%s : [%s]\n") % "Device" % _pt.get<std::string>("platform.bdf");
  _output << std::endl;
  _output << "Flash properties\n";
  _output << boost::format("  %-20s : %s\n") % "Type" % _pt.get<std::string>("platform.flash_type", "N/A");

  _output << std::endl;
  _output << "Flashable partition running on FPGA\n";
  _output << boost::format("  %-20s : %s\n") % "Platform" % _pt.get<std::string>("platform.shell_on_fpga.vbnv", "N/A");
  _output << boost::format("  %-20s : %s\n") % "SC Version" % _pt.get<std::string>("platform.shell_on_fpga.sc_version", "N/A");
  _output << boost::format("  %-20s : %s\n") % "Platform ID" % _pt.get<std::string>("platform.shell_on_fpga.id", "N/A");
  
  _output << std::endl;
  _output << "Flashable partitions installed in system\n";
  _output << boost::format("  %-20s : %s\n") % "Platform" % _pt.get<std::string>("platform.installed_shell.vbnv", "N/A");
  _output << boost::format("  %-20s : %s\n") % "SC Version" % _pt.get<std::string>("platform.installed_shell.sc_version", "N/A");
  _output << boost::format("  %-20s : %s\n") % "Platform ID" % _pt.get<std::string>("platform.installed_shell.id", "N/A");

  _output << "----------------------------------------------------\n"
          << shell_status(_pt.get<bool>("platform.shell_upto_date", ""), 
                          _pt.get<bool>("platform.sc_upto_date", ""), 
                          _pt.get<int>("platform.number_of_installed_shells"));
}
