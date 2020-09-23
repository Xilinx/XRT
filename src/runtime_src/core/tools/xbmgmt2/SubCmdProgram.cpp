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
#include "SubCmdProgram.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenus.h"
#include "tools/common/ProgressBar.h"
namespace XBU = XBUtilities;

#include "xrt.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/query_requests.h"
#include "core/common/message.h"
#include "core/common/utils.h"
#include "flash/flasher.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// ---- Reports ------
#include "tools/common/Report.h"
#include "tools/common/ReportHost.h"
#include "ReportPlatform.h"

// System - Include Files
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <ctime>
#include <locale>
#include <fcntl.h>

#ifdef _WIN32
#pragma warning(disable : 4996) //std::asctime
#endif


// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {
/*
 * Update shell on the board for auto flash
 */
static void 
update_shell(unsigned int index, const std::string& primary, const std::string& secondary)
{
  Flasher flasher(index);
  if(!flasher.isValid())
    throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % index));

  if (primary.empty())
    throw xrt_core::error("Shell not specified");

  auto pri = std::make_unique<firmwareImage>(primary.c_str(), MCS_FIRMWARE_PRIMARY);
  if (pri->fail())
    throw xrt_core::error(boost::str(boost::format("Failed to read %s") % primary));

  std::unique_ptr<firmwareImage> sec;
  if (!secondary.empty()) {
    sec = std::make_unique<firmwareImage>(secondary.c_str(),
      MCS_FIRMWARE_SECONDARY);
    if (sec->fail())
      sec = nullptr;
  }
  
  if (flasher.upgradeFirmware("", pri.get(), sec.get()) != 0)
    throw xrt_core::error("Failed to update shell");
  
  std::cout << boost::format("%-8s : %s \n") % "INFO" % "Shell is updated successfully.";
}

/*
 * Update shell on the board for manual flash
 */
static void 
update_shell(unsigned int index, const std::string& flashType,
  const std::string& primary, const std::string& secondary)
{
  if (!flashType.empty()) {
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", 
        "Overriding flash mode is not recommended.\nYou may damage your card with this option.");
  }

  Flasher flasher(index);
  if(!flasher.isValid())
    throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % index));

  if (primary.empty())
    throw xrt_core::error("Shell not specified");

  auto pri = std::make_unique<firmwareImage>(primary.c_str(), MCS_FIRMWARE_PRIMARY);
  if (pri->fail())
    throw xrt_core::error(boost::str(boost::format("Failed to read %s") % primary));

  std::unique_ptr<firmwareImage> sec;
  if (!secondary.empty()) {
    sec = std::make_unique<firmwareImage>(secondary.c_str(), MCS_FIRMWARE_SECONDARY);
    if (sec->fail())
      throw xrt_core::error(boost::str(boost::format("Failed to read %s") % secondary));
  }

  if (flasher.upgradeFirmware(flashType, pri.get(), sec.get()) != 0)
    throw xrt_core::error("Failed to update shell");
  
  std::cout << boost::format("%-8s : %s \n") % "INFO" % "Shell is updated successfully.";
  std::cout << "****************************************************\n";
  std::cout << "Cold reboot machine to load the new image on card.\n";
  std::cout << "****************************************************\n";
}

/*
 * Update SC firmware on the board
 */
static void 
update_SC(unsigned int  index, const std::string& file)
{
  Flasher flasher(index);
  if(!flasher.isValid())
    throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % index));

  std::unique_ptr<firmwareImage> bmc =
    std::make_unique<firmwareImage>(file.c_str(), BMC_FIRMWARE);
  if (bmc->fail())
    throw xrt_core::error(boost::str(boost::format("Failed to read %s") % file));

  if (flasher.upgradeBMCFirmware(bmc.get()) != 0)
    throw xrt_core::error("Failed to update SC");
}

/* 
 * Helper function for header info
 */
static std::string 
file_size(const std::string & _file) 
{
  std::ifstream in(_file.c_str(), std::ifstream::ate | std::ifstream::binary);
  auto total_size = std::to_string(in.tellg()); 
  int strSize = static_cast<int>(total_size.size());

  //if file size is > 3 digits, insert commas after every 3 digits
  static const int COMMA_SPACING = 3;
  if (strSize > COMMA_SPACING) {
   for (int i = (strSize - COMMA_SPACING);  i > 0;  i -= COMMA_SPACING)
     total_size.insert(static_cast<size_t>(i), 1, ',');
  }
  
  return total_size + std::string(" bytes");
}

/* 
 * Helper function for header info
 */
static std::pair<std::string, std::string> 
deployment_path_and_filename(std::string file)
{
  using tokenizer = boost::tokenizer< boost::char_separator<char> >;
  boost::char_separator<char> sep("\\/");
  tokenizer tokens(file, sep);
  std::string dsafile = "";
  for (auto tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
  	if ((std::string(*tok_iter).find(XSABIN_FILE_SUFFIX) != std::string::npos) 
          || (std::string(*tok_iter).find(DSABIN_FILE_SUFFIX) != std::string::npos))
          dsafile = *tok_iter;
  }
  auto pos = file.rfind('/') != std::string::npos ? file.rfind('/') : file.rfind('\\');
  std::string path = file.erase(pos);

  return std::make_pair(dsafile, path);
}

/* 
 * Helper function for header info
 */
static std::string 
get_file_timestamp(const std::string & _file) 
{
  boost::filesystem::path p(_file);
	if (!boost::filesystem::exists(p)) {
		throw xrt_core::error("Invalid platform path.");
	}
  std::time_t ftime = boost::filesystem::last_write_time(boost::filesystem::path(_file));
  std::string timeStr(std::asctime(std::localtime(&ftime)));
  timeStr.pop_back();  // Remove the new-line character that gets inserted by asctime.
  return timeStr;
}

static void
pretty_print_platform_info(const boost::property_tree::ptree& _ptDevice)
{
  std::cout << boost::format("%s : [%s]\n") % "Device" % _ptDevice.get<std::string>("platform.bdf");
  std::cout << std::endl;
  std::cout << "Current Configuration\n";

  std::cout << boost::format("  %-20s : %s\n") % "Platform" % _ptDevice.get<std::string>("platform.current_shell.vbnv", "N/A");
  std::cout << boost::format("  %-20s : %s\n") % "SC Version" % _ptDevice.get<std::string>("platform.current_shell.sc_version", "N/A");
  std::cout << boost::format("  %-20s : %s\n") % "Platform ID" % _ptDevice.get<std::string>("platform.current_shell.id", "N/A");
  std::cout << std::endl;
  std::cout << "\nIncoming Configuration\n";
  const boost::property_tree::ptree& available_shells = _ptDevice.get_child("platform.available_shells");
  // if multiple shells are installed, do not proceed
  if( available_shells.size() > 1)
      throw xrt_core::error("Auto update is not possible when multiple shells are installed on the system. Please use --image option to specify the path of a particular flash image.");

  const boost::property_tree::ptree& available_shell = available_shells.front().second;
  std::pair <std::string, std::string> s = deployment_path_and_filename(available_shell.get<std::string>("file"));
  std::cout << boost::format("  %-20s : %s\n") % "Deployment File" % s.first;
  std::cout << boost::format("  %-20s : %s\n") % "Deployment Directory" % s.second;
  std::cout << boost::format("  %-20s : %s\n") % "Size" % file_size(available_shell.get<std::string>("file").c_str());
  std::cout << boost::format("  %-20s : %s\n\n") % "Timestamp" % get_file_timestamp(available_shell.get<std::string>("file").c_str());

  std::cout << boost::format("  %-20s : %s\n") % "Platform" % available_shell.get<std::string>("vbnv", "N/A");
  std::cout << boost::format("  %-20s : %s\n") % "SC Version" % available_shell.get<std::string>("sc_version", "N/A");
  std::cout << boost::format("  %-20s : %s\n") % "Platform ID" % available_shell.get<std::string>("id", "N/A");
}

static void
report_status(xrt_core::device_collection& deviceCollection, boost::property_tree::ptree& _pt) 
{
  std::vector<std::string> elementsFilter;
  //get platform report for all the devices
  std::cout << "----------------------------------------------------\n";
  for (const auto & device : deviceCollection) {
    boost::property_tree::ptree _ptDevice;
    auto _rep = std::make_unique<ReportPlatform>();
    _rep->getPropertyTreeInternal(device.get(), _ptDevice);
    _pt.push_back(std::make_pair(std::to_string(device->get_device_id()), _ptDevice));
    pretty_print_platform_info(_ptDevice);
    std::cout << "----------------------------------------------------\n";
  }

  std::stringstream action_list;
  for (const auto & device : deviceCollection) {
    if (!_pt.get<bool>(std::to_string(device->get_device_id()) + ".platform.status.shell"))
      action_list << boost::format("  [%s] : Program shell (FLASH) image\n") % _pt.get<std::string>(std::to_string(device->get_device_id())+".platform.bdf");
    if (!_pt.get<bool>(std::to_string(device->get_device_id())+".platform.status.sc"))
      action_list << boost::format("  [%s] : Program Satellite Controller (SC) image\n") % _pt.get<std::string>(std::to_string(device->get_device_id())+".platform.bdf");
  }
  
  if(!action_list.str().empty()) {
    std::cout << "Actions to perform:\n" << action_list.str();
    std::cout << "----------------------------------------------------\n";
  }

}

/* 
 * Flash shell and sc firmware
 * Helper method for auto_flash
 */
static int 
updateShellAndSC(unsigned int  boardIdx, DSAInfo& candidate, bool& reboot)
{
  reboot = false;

  Flasher flasher(boardIdx);

  bool same_dsa = false;
  bool same_bmc = false;
  DSAInfo current = flasher.getOnBoardDSA();
  if (!current.name.empty()) {
    same_dsa = (candidate.name == current.name &&
      candidate.matchId(current));

    // Always update Arista devices
    if (candidate.vendor_id == ARISTA_ID)
        same_dsa = false;

    // getOnBoardDSA() returns an empty bmcVer in the case there is no SC,
    // so do not update
    if (current.bmcVer.empty())
        same_bmc = true;
    else
        same_bmc = (candidate.bmcVer == current.bmcVer);
  }
  if (same_dsa && same_bmc) {
    std::cout << "update not needed" << std::endl;
    return 0;
  }

  if (!same_bmc) {
    std::cout << "Updating SC firmware on card[" << flasher.sGetDBDF() <<
      "]" << std::endl;
    update_SC(boardIdx, candidate.file);
  }

  if (!same_dsa) {
    std::cout << boost::format("[%s] : Updating shell\n") % flasher.sGetDBDF();
    update_shell(boardIdx, candidate.file, candidate.file);
    reboot = true;
  }

  if (!same_dsa && !reboot)
    return -EINVAL;

  return 0;
}

static std::string 
getBDF(unsigned int index)
{
  auto dev =xrt_core::get_mgmtpf_device(index);
  auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(dev);
  return xrt_core::query::pcie_bdf::to_string(bdf);
}

/* 
 * Update shell and sc firmware on the card automatically
 */
static void 
auto_flash(xrt_core::device_collection& deviceCollection, bool force) 
{
  //report status of all the devices
  boost::property_tree::ptree _pt;
  report_status(deviceCollection, _pt);

  // Collect all indexes of boards need updating
  std::vector<std::pair<unsigned int , DSAInfo>> boardsToUpdate;
  for (const auto & device : deviceCollection) {
    DSAInfo dsa(_pt.get_child(std::to_string(device->get_device_id()) + ".platform.available_shells").front().second.get<std::string>("file"));
    //if the shell is not up-to-date and dsa has a flash image, queue the board for update
    bool same_shell = _pt.get<bool>(std::to_string(device->get_device_id()) + ".platform.status.shell");
    bool same_sc = _pt.get<bool>(std::to_string(device->get_device_id()) + ".platform.status.sc");

    // Always update Arista devices
    auto vendor = xrt_core::device_query<xrt_core::query::pcie_vendor>(device);
    if (vendor == ARISTA_ID)
        same_shell = false;

    if (!same_shell || !same_sc) {
      if(!dsa.hasFlashImage)
        throw xrt_core::error("Flash image is not available");
      boardsToUpdate.push_back(std::make_pair(device->get_device_id(), dsa));
    }
  }

  // Continue to flash whatever we have collected in boardsToUpdate.
  uint16_t success = 0;
  bool needreboot = false;
  std::stringstream report_status;
  if (!boardsToUpdate.empty()) {

    // Prompt user about what boards will be updated and ask for permission.
    if(!force && !XBU::can_proceed())
      return;

    // Perform DSA and BMC updating
    for (auto& p : boardsToUpdate) {
      bool reboot;
      std::cout << std::endl;
      try {
        updateShellAndSC(p.first, p.second, reboot);
        report_status << boost::format("  [%s] : Successfully flashed\n") % getBDF(p.first);
        success++;
      } catch (const xrt_core::error& e) {
        std::cerr << boost::format("ERROR: %s\n") % e.what();
      }
      needreboot |= reboot;
    }
  }
  std::cout << "----------------------------------------------------\n";
  std::cout << "Report\n";
  //report status of cards
  std::cout << report_status.str();
  if (boardsToUpdate.size() == 0) {
    std::cout << "\nCard(s) up-to-date and do not need to be flashed." << std::endl;
    return;
  }

  if (success != 0) {
    std::cout << "\n" << success << " Card(s) flashed successfully." << std::endl; 
  } else {
    std::cout << "\nNo cards were flashed." << std::endl; 
  }

  if (needreboot) {
    std::cout << "****************************************************\n";
    std::cout << "Cold reboot machine to load the new image on card(s).\n";
    std::cout << "****************************************************\n";
  }

  if (success != boardsToUpdate.size()) {
    std::cout << "WARNING:" << boardsToUpdate.size()-success << " Card(s) not flashed. " << std::endl;
  }
}

static void
program_plp(std::shared_ptr<xrt_core::device> dev, const std::string& partition)
{
  std::ifstream stream(partition.c_str(), std::ios_base::binary);
  if (!stream.is_open())
    throw xrt_core::error(boost::str(boost::format("Cannot open %s") % partition));

  //size of the stream
  stream.seekg(0, stream.end);
  int total_size = static_cast<int>(stream.tellg());
  stream.seekg(0, stream.beg);

  //copy stream into a vector
  std::vector<char> buffer(total_size);
  stream.read(buffer.data(), total_size);

  xrt_core::program_plp(dev, buffer);
  std::cout << "Programmed PLP successfully" << std::endl;
}

}
//end anonymous namespace

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdProgram::SubCmdProgram(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("program", 
             "Update device and/or Satallite Controler (SC) firmware image for a given device")
{
  const std::string longDescription = "Updates the flash image for the device and/or the Satallite Controller (SC) firmware image for a given device.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdProgram::execute(const SubCmdOptions& _options) const
// Reference Command:  [-d card] [-r region] -p xclbin
//                     Download the accelerator program for card 2
//                       xbutil program -d 2 -p a.xclbin
{
  XBU::verbose("SubCommand: program");

  XBU::verbose("Option(s):");
  for (auto & aString : _options) {
    std::string msg = "   ";
    msg += aString;
    XBU::verbose(msg);
  }

  // -- Retrieve and parse the subcommand options -----------------------------
  std::vector<std::string> device;
  std::string plp = "";
  std::string update = "";
  std::string flashType = "";
  std::vector<std::string> image;
  bool revertToGolden = false;
  bool help = false;
  bool force = false;

  po::options_description commonOptions("Common Options");  
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(device)>(&device)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.  A value of 'all' indicates that every found device should be examined.")
    ("partition", boost::program_options::value<decltype(plp)>(&plp), "The partition to be loaded.  Valid values:\n"
                                                                      "  Name (and path) of the partition.\n"
                                                                      "  Parition's UUID")
    ("update", boost::program_options::value<decltype(update)>(&update)->implicit_value("all"), "Update the persistent images.  Value values:\n"
                                                                         "  ALL   - All images will be updated"
                                                                     /*  "  FLASH - Flash image\n"
                                                                         "  SC    - Satellite controller"*/)
    ("image", boost::program_options::value<decltype(image)>(&image)->multitoken(), "Specifies an image to use used to update the persistent device(s).  Value values:\n"
                                                                      "  Name (and path) to the mcs image on disk\n"
                                                                      "  Name (and path) to the xsabin image on disk\n"
                                                                      "Note: Multiple images can be specified separated by a space")
    ("force,f", boost::program_options::bool_switch(&force), "Force update the flash image")
    ("revert-to-golden", boost::program_options::bool_switch(&revertToGolden), "Resets the FPGA PROM back to the factory image.  Note: This currently only applies to the flash image.")
    ("help,h", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  po::options_description hiddenOptions("Hidden Options");  
  hiddenOptions.add_options()
    ("flash-type", boost::program_options::value<decltype(flashType)>(&flashType), "Overrides the flash mode. Use with caution.  Value values:\n"
                                                                    "  ospi\n"
                                                                    "  ospi_versal")
  ;

  po::options_description allOptions("All Options");  

  allOptions.add(commonOptions);
  allOptions.add(hiddenOptions);


  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(allOptions).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(commonOptions, hiddenOptions);
    throw; // Re-throw exception
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(boost::str(boost::format("  Update: %s") % update));

  // -- process "device" option -----------------------------------------------
  //enforce device specification
  if(device.empty()) {
    std::cout << "\nERROR: Device not specified.\n";
    std::cout << "\nList of available devices:" << std::endl;
    boost::property_tree::ptree available_devices = XBU::get_available_devices(false);
    for(auto& kd : available_devices) {
      boost::property_tree::ptree& dev = kd.second;
      std::cout << boost::format("  [%s] : %s\n") % dev.get<std::string>("bdf") % dev.get<std::string>("vbnv");
    }
    std::cout << std::endl;
    return;
  }

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  for (const auto & deviceName : device) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  XBU::collect_devices(deviceNames, false /*inUserDomain*/, deviceCollection);

  if(force) {
    //force is a sub-option of update
    if(update.empty())
      throw xrt_core::error("Usage: xbmgmt program --device='0000:00:00.0' --update --force'");
  }

  // TODO: Added mutually exclusive code for image, update, and revert-to-golden action.

  if(!image.empty()) {
    //image is a sub-option of update
    if(update.empty())
      throw xrt_core::error("Usage: xbmgmt program --device='0000:00:00.0' --update --image='/path/to/flash_image'");
    //allow only 1 device to be manually flashed at a time
    if(deviceCollection.size() != 1)
      throw xrt_core::error("Please specify a single device to be flashed");
    //we support only 2 flash images atm
    if(image.size() > 2)
      throw xrt_core::error("Please specify either 1 or 2 flash images");
    if(!XBU::can_proceed())
      return;
    update_shell(deviceCollection.front()->get_device_id(), flashType, image.front(), (image.size() == 2 ? image[1]: ""));
    return;
  }

  if (!update.empty()) {
    XBU::verbose("Sub command: --update");
    std::string empty = "";
    if(update.compare("all") == 0)
      auto_flash(deviceCollection, force);
    else if(update.compare("flash") == 0)
      throw xrt_core::error("Platform only update is not supported");
    else if(update.compare("sc") == 0)
      throw xrt_core::error("SC only update is not supported");
    else 
      throw xrt_core::error("Please specify a valid value");
    return;
  }
  
  // -- process "revert-to-golden" option ---------------------------------------
  if(revertToGolden) {
    XBU::verbose("Sub command: --revert-to-golden");

    std::vector<Flasher> flasher_list;
    for (const auto & dev : deviceCollection) {
      //collect information of all the cards that will be reset
      Flasher flasher(dev->get_device_id());
      if(!flasher.isValid()) {
        xrt_core::error(boost::str(boost::format("%d is an invalid index") % dev->get_device_id()));
        continue;
      }
      std::cout << boost::format("%-8s : %s %s %s \n") % "INFO" % "Resetting card [" 
        % flasher.sGetDBDF() % "] back to factory mode.";
      flasher_list.push_back(flasher);
    }
    
    //ask user's permission
    if(!XBU::can_proceed())
      return;
    
    for(auto& f : flasher_list) {
      f.upgradeFirmware("", nullptr, nullptr);
      std::cout << boost::format("%-8s : %s %s %s\n") % "INFO" % "Shell on [" % f.sGetDBDF() % "] is reset successfully." ;
    }

    std::cout << "****************************************************\n";
    std::cout << "Cold reboot machine to load the new image on card(s).\n";
    std::cout << "****************************************************\n";
    return;
  }

  // -- process "plp" option ---------------------------------------
  if(!plp.empty()) {
    XBU::verbose(boost::str(boost::format("  plp: %s") % plp));
    //only 1 card and name
    if(deviceCollection.size() > 1)
      throw xrt_core::error("Please specify a single device");
    auto dev = deviceCollection.front();

    Flasher flasher(dev->get_device_id());
    if(!flasher.isValid())
      throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % dev->get_device_id()));

    if(xrt_core::device_query<xrt_core::query::interface_uuids>(dev).empty())
      throw xrt_core::error("Can not get BLP interface uuid. Please make sure corresponding BLP package is installed.");

    std::vector<DSAInfo> available_partitions;
    auto installedDSAs = firmwareImage::getIntalledDSAs();
    for (const auto& dsa : installedDSAs) {
      if (dsa.uuids.size() != 0) {
        if (dsa.name.compare(plp) == 0 || dsa.matchIntId(plp))
          available_partitions.push_back(dsa);
      }
    }

    if (available_partitions.empty())
      throw xrt_core::error("No matching partition found");

    if (available_partitions.size() > 1) {
      std::stringstream errmsg;
      errmsg << "Multiple partitions found with the same name. Please specify the UUID\n";
      for (const auto& d : available_partitions)
        errmsg << boost::format("  - %s [0x%x]\n") % d.name % d.timestamp;
      throw xrt_core::error(errmsg.str());
    }

    DSAInfo dsa(available_partitions.front());
    //TO_DO: add a report for plp before asking permission to proceed. Replace following 2 lines
    std::cout << "Programming PLP on Card [" << flasher.sGetDBDF() << "]..." << std::endl;
    std::cout << "Partition file: " << dsa.file << std::endl;
    
    //ask user's permission
    for (const auto& uuid : dsa.uuids) {
      //check if plp is compatible with the installed blp
      if (xrt_core::device_query<xrt_core::query::interface_uuids>(dev).front().compare(uuid) == 0) {
        program_plp(dev, dsa.file);
        return;
      }
    }
    throw xrt_core::error("uuid does not match BLP");
  }

  std::cout << "\nERROR: Missing flash operation.  No action taken.\n\n";
  printHelp(commonOptions, hiddenOptions);
}
