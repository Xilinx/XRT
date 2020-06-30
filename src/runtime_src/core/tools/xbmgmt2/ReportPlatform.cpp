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
#include "core/common/query_requests.h"

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

  Flasher f(_pDevice->get_device_id());
  std::vector<DSAInfo> availableDSAs = f.getInstalledDSA();

  BoardInfo info;
  f.getBoardInfo(info);
  //create information tree for a device
  _pt.put("platform.bdf", on_board_dev_info.get<std::string>("bdf"));
  _pt.put("platform.flash_type", on_board_platform_info.get<std::string>("flash_type", "N/A"));
  _pt.put("platform.hardware.serial_num", info.mSerialNum);
  //Flashable partition running on FPGA
  std::vector<std::string> logic_uuids, interface_uuids;
  try {
    logic_uuids = xrt_core::device_query<xrt_core::query::logic_uuids>(_pDevice);
    } catch (...) {}
  try {
    interface_uuids = xrt_core::device_query<xrt_core::query::interface_uuids>(_pDevice);
    } catch (...) {}
  
  //check if 2RP
  if(!logic_uuids.empty() && !interface_uuids.empty()) {
    for(unsigned int i = 0; i < logic_uuids.size(); i++) {
      DSAInfo part("", NULL_TIMESTAMP, logic_uuids[i], ""); 
      _pt.put("platform.current_shell.vbnv", part.name);
      _pt.put("platform.current_shell.logic-uuid", logic_uuids[i]);
      _pt.put("platform.current_shell.interface-uuid", interface_uuids[i]);
      _pt.put("platform.current_shell.id", (boost::format("0x%x") % part.timestamp));
    }
  } else { //1RP
    _pt.put("platform.current_shell.vbnv", on_board_rom_info.get<std::string>("vbnv", "N/A"));
    _pt.put("platform.current_shell.id", on_board_rom_info.get<std::string>("id", "N/A"));
  }

  std::string _scVer = on_board_xmc_info.get<std::string>("sc_version");
  if(_scVer.empty())
    _scVer = info.mBMCVer;
  _pt.put("platform.current_shell.sc_version", _scVer);

  //Flashable partitions installed in system
  boost::property_tree::ptree _ptAvailableShells;
  for(unsigned int i = 0; i < availableDSAs.size(); i++) {
    boost::property_tree::ptree _ptAvailableShell;
    DSAInfo installedDSA = availableDSAs[i];
    _ptAvailableShell.put("vbnv", installedDSA.name);
    _ptAvailableShell.put("sc_version", installedDSA.bmcVer);
    _ptAvailableShell.put("id", (boost::format("0x%x") % installedDSA.timestamp));
    _ptAvailableShell.put("file", installedDSA.file);
    _pt.put("platform.status.shell", same_shell( _pt.get<std::string>("platform.current_shell.vbnv", ""), 
              _pt.get<std::string>("platform.current_shell.id", ""), installedDSA));
    _pt.put("platform.status.sc", same_sc( _pt.get<std::string>("platform.current_shell.sc_version", ""), 
             installedDSA));
     _ptAvailableShells.push_back( std::make_pair("", _ptAvailableShell) );
  }
_pt.put_child("platform.available_shells", _ptAvailableShells);
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
  _output << boost::format("  %-20s : %s\n") % "Serial Number" % _pt.get<std::string>("platform.hardware.serial_num", "N/A");
  _output << std::endl;
  _output << "Flashable partition running on FPGA\n";
  _output << boost::format("  %-20s : %s\n") % "Platform" % _pt.get<std::string>("platform.current_shell.vbnv", "N/A");
  _output << boost::format("  %-20s : %s\n") % "SC Version" % _pt.get<std::string>("platform.current_shell.sc_version", "N/A");
  _output << boost::format("  %-20s : %x\n") % "Platform ID" % _pt.get<std::string>("platform.current_shell.id", "N/A");
  
  _output << std::endl;
  _output << "Flashable partitions installed in system\n"; 
  boost::property_tree::ptree& available_shells = _pt.get_child("platform.available_shells");
  for(auto& kv : available_shells) {
    // std::string prefix = "platform.available_shells." + std::to_string(i);
    boost::property_tree::ptree& available_shell = kv.second;
    _output << boost::format("  %-20s : %s\n") % "Platform" % available_shell.get<std::string>("vbnv", "N/A");
    _output << boost::format("  %-20s : %s\n") % "SC Version" % available_shell.get<std::string>("sc_version", "N/A");
    _output << boost::format("  %-20s : %x\n") % "Platform ID" % available_shell.get<std::string>("id", "N/A") << "\n";
  }


  _output << "----------------------------------------------------\n"
          << shell_status(_pt.get<bool>("platform.status.shell"), 
                          _pt.get<bool>("platform.status.sc"),  static_cast<int>(available_shells.size()));
}
