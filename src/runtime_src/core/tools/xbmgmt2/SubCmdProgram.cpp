/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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
#include "tools/common/Process.h"
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
    throw xrt_core::error("Failed to update base");
  
  std::cout << boost::format("%-8s : %s \n") % "INFO" % "Base flash image has been programmed successfully.";
}

/*
 * Update shell on the board for manual flash
 */
static void 
update_shell(unsigned int index, const std::string& flashType,
  const std::string& primary, const std::string& secondary)
{
  if (!flashType.empty()) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", 
        "Overriding flash mode is not recommended.\nYou may damage your device with this option.");
  }

  Flasher flasher(index);
  if(!flasher.isValid())
    throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % index));

  if (primary.empty())
    throw xrt_core::error("Base not specified");

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
    throw xrt_core::error("Failed to update base");
  
  std::cout << boost::format("%-8s : %s \n") % "INFO" % "Base flash image has been programmed successfully.";
  std::cout << "****************************************************\n";
  std::cout << "Cold reboot machine to load the new image on device.\n";
  std::cout << "****************************************************\n";
}

static std::string 
getBDF(unsigned int index)
{
  auto dev = xrt_core::get_mgmtpf_device(index);
  auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(dev);
  return xrt_core::query::pcie_bdf::to_string(bdf);
}


static bool
is_SC_fixed(unsigned int index)
{
  try {
    auto device = xrt_core::get_mgmtpf_device(index);
    return xrt_core::device_query<xrt_core::query::is_sc_fixed>(device);
  }
  catch (...) {
    //TODO Catching all the exceptions for now. We may need to catch specific exceptions
    //Work-around. Assume that sc is not fixed if above query throws an exception
    return false;
  }
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

  auto dev = xrt_core::get_mgmtpf_device(index);

  // If SC is fixed, stop flashing immediately
  if (is_SC_fixed(index)) 
    throw xrt_core::error("SC is fixed, unable to flash image.");

  //don't trigger reset for u30. let python helper handle everything
  
  if (xrt_core::device_query<xrt_core::query::rom_vbnv>(dev).find("_u30_") != std::string::npos) {
    std::ostringstream os_stdout;
    std::ostringstream os_stderr;
    const std::string scFlashPath = "/opt/xilinx/xrt/bin/unwrapped/_scflash.py";
    std::vector<std::string> args = { "-y", "-d", getBDF(index), "-p", file };
    
    int exit_code = XBU::runScript("python", scFlashPath, args, "Programming SC ", "SC Programmed", 120, os_stdout, os_stderr, false);

    if (exit_code != 0) {
      std::string err_msg = "ERROR: " + os_stdout.str() + "\n" + os_stderr.str() + "\n";
      throw xrt_core::error(err_msg);
    }
    return;
  }

  std::unique_ptr<firmwareImage> bmc = std::make_unique<firmwareImage>(file.c_str(), BMC_FIRMWARE);

  if (bmc->fail())
    throw xrt_core::error(boost::str(boost::format("Failed to read %s") % file));

  if (flasher.upgradeBMCFirmware(bmc.get()) != 0)
    throw xrt_core::error("Failed to update SC flash image");
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
pretty_print_platform_info(const boost::property_tree::ptree& _ptDevice, const std::string& vbnv)
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

  boost::property_tree::ptree platform_to_flash;
  for(auto& image : available_shells) {
    if((image.second.get<std::string>("vbnv")).compare(vbnv) == 0) {
      platform_to_flash = image.second;
      break;
    }
  }
  std::pair <std::string, std::string> s = deployment_path_and_filename(platform_to_flash.get<std::string>("file"));
  std::cout << boost::format("  %-20s : %s\n") % "Deployment File" % s.first;
  std::cout << boost::format("  %-20s : %s\n") % "Deployment Directory" % s.second;
  std::cout << boost::format("  %-20s : %s\n") % "Size" % file_size(platform_to_flash.get<std::string>("file").c_str());
  std::cout << boost::format("  %-20s : %s\n\n") % "Timestamp" % get_file_timestamp(platform_to_flash.get<std::string>("file").c_str());

  std::cout << boost::format("  %-20s : %s\n") % "Platform" % platform_to_flash.get<std::string>("vbnv", "N/A");
  std::cout << boost::format("  %-20s : %s\n") % "SC Version" % platform_to_flash.get<std::string>("sc_version", "N/A");
  auto logic_uuid = platform_to_flash.get<std::string>("logic-uuid", "");
  if (!logic_uuid.empty()) {
    std::cout << boost::format("  %-20s : %s\n") % "Platform UUID" % logic_uuid;
  } else {
    std::cout << boost::format("  %-20s : %s\n") % "Platform ID" % platform_to_flash.get<std::string>("id", "N/A");
  }
}

static void
report_status(std::shared_ptr<xrt_core::device> & workingDevice, const std::string& vbnv,
              boost::property_tree::ptree& pt_device) 
{
  std::cout << "----------------------------------------------------\n";
  pretty_print_platform_info(pt_device, vbnv);
  std::cout << "----------------------------------------------------\n";

  std::stringstream action_list;

  if (!pt_device.get<bool>("platform.status.shell"))
    action_list << boost::format("  [%s] : Program base (FLASH) image\n") % pt_device.get<std::string>("platform.bdf");

  if (!pt_device.get<bool>("platform.status.sc"))
    action_list << boost::format("  [%s] : Program Satellite Controller (SC) image\n") % pt_device.get<std::string>("platform.bdf");
  
  if(!action_list.str().empty()) {
    std::cout << "Actions to perform:\n" << action_list.str();
    std::cout << "----------------------------------------------------\n";
  }
}


static bool
are_shells_equal(const DSAInfo& candidate, const DSAInfo& current)
{
  if (current.dsaname().empty())
    throw std::runtime_error("Current shell name is empty.");

  return ((candidate.dsaname() == current.dsaname()) && candidate.matchId(current));
}


static bool
are_scs_equal(const DSAInfo& candidate, const DSAInfo& current)
{
  if (current.dsaname().empty())
    throw std::runtime_error("Current shell name is empty.");

  return ((current.bmcVer.compare("INACTIVE") == 0) ||
          (candidate.bmc_ver() == current.bmc_ver()));
}

static bool 
update_sc(unsigned int boardIdx, DSAInfo& candidate)
{
  Flasher flasher(boardIdx);

  // Determine if the SC images are the same
  bool same_bmc = false;
  DSAInfo current = flasher.getOnBoardDSA();
  if (!current.dsaname().empty()) 
    same_bmc = are_scs_equal(candidate, current);

  // -- Some DRCs (Design Rule Checks) --
  // Is the SC present
  if (current.bmc_ver().empty()) {
     std::cout << "INFO: Satellite controller is not present.\n";
     return false;
  }
  
  // Can the SC be programmed  
  if (is_SC_fixed(boardIdx)) {
    std::cout << "INFO: Fixed Satellite Controller.\n";
    return false;
  }
   
  // Check to see if force is being used
  if ((same_bmc == true) && (XBU::getForce() == true)) {
    std::cout << "INFO: Forcing flashing of the Satellite Controller (SC) image (Force flag is set).\n";
    same_bmc = false;
  }

  // Don't program the same images
  if (same_bmc == true) {
    std::cout << "INFO: Satellite Controller (SC) images are the same.\n";
    return false;
  }

  // -- Program the SC image --
  boost::format programFmt("[%s] : %s...\n");
  std::cout << programFmt % flasher.sGetDBDF() % "Updating Satellite Controller (SC) firmware flash image";
  update_SC(boardIdx, candidate.file);
  std::cout << std::endl;

  return true;
}


/* 
 * Flash shell and sc firmware
 * Helper method for auto_flash
 */
static bool  
update_shell(unsigned int boardIdx, DSAInfo& candidate)
{
  Flasher flasher(boardIdx);

  // Determine if the shells are the same
  bool same_dsa = false;
  DSAInfo current = flasher.getOnBoardDSA();
  if (!current.dsaname().empty()) 
    same_dsa = are_shells_equal(candidate, current);

  // -- Some DRCs (Design Rule Checks) --
  // Always update Arista devices
  if (candidate.vendor_id == ARISTA_ID) {
    std::cout << "INFO: Arista device (Force flashing).\n";
    same_dsa = false;
  }

  // Check to see if force is being used
  if ((same_dsa == true) && (XBU::getForce() == true)) {
    std::cout << "INFO: Forcing flashing of the base (e.g., shell) image (Force flag is set).\n";
    same_dsa = false;
  }

  // Don't program the same images
  if (same_dsa == true) {
    std::cout << "INFO: Base (e.g., shell) flash images are the same.\n";
    return false;
  }

  // Program the shell
  boost::format programFmt("[%s] : %s...\n");
  std::cout << programFmt % flasher.sGetDBDF() % "Updating base (e.g., shell) flash image";
  update_shell(boardIdx, candidate.file, candidate.file);
  return true;
}

/* 
 * Update shell and sc firmware on the device automatically 
 * Refactor code to support only 1 device. 
 */
static void 
auto_flash(std::shared_ptr<xrt_core::device> & workingDevice, const std::string& image = "") 
{
  // Get platform information
  boost::property_tree::ptree pt;
  boost::property_tree::ptree ptDevice;
  auto rep = std::make_unique<ReportPlatform>();
  rep->getPropertyTreeInternal(workingDevice.get(), ptDevice);
  pt.push_back(std::make_pair(std::to_string(workingDevice->get_device_id()), ptDevice));

  // Collect all indexes of boards need updating
  std::vector<std::pair<unsigned int , DSAInfo>> boardsToUpdate;

  std::string image_path;
  if(image.empty()) {
    static boost::property_tree::ptree ptEmpty;
    auto available_shells = pt.get_child(std::to_string(workingDevice->get_device_id()) + ".platform.available_shells", ptEmpty);

    // Check if any base packages are available
    if (available_shells.empty()) {
      std::cout << "ERROR: No base (e.g., shell) images installed on the server. Operation canceled.\n";
      throw xrt_core::error(std::errc::operation_canceled);
    }

    // Check if multiple base packages are available
    if (available_shells.size() > 1) {
      std::cout << "ERROR: Multiple images installed on the server. Please specify a single image using --image option. Operation canceled.\n";
      throw xrt_core::error(std::errc::operation_canceled);
    }
    image_path = available_shells.front().second.get<std::string>("file");
  }
  else {
    //iterate over installed shells
    //check the vbnv against the vnbv passed in by the user
    auto installedShells = firmwareImage::getIntalledDSAs();
    int multiple_shells = 0;
      
    for(auto const& shell : installedShells) {
      if(image.compare(shell.name) == 0) {
        multiple_shells++;
        image_path = shell.file;
      }
    }

    //if multiple shells with the same vbnv are installed on the system, we don't want to 
    //blindly update the device. in this case, the user needs to specify the complete path
    if(multiple_shells > 1) 
      throw xrt_core::error("Specified base matched mutiple installed bases. Please specify the full path.");

    if(multiple_shells == 0) 
      throw xrt_core::error("Specified base not found on the system");
  }
    
  DSAInfo dsa(image_path);

  // If the shell is not up-to-date and dsa has a flash image, queue the board for update
  boost::property_tree::ptree& pt_dev = pt.get_child(std::to_string(workingDevice->get_device_id()));
  bool same_shell = (dsa.name == pt_dev.get<std::string>("platform.current_shell.vbnv", ""))
                      && (dsa.matchId(pt_dev.get<std::string>("platform.current_shell.id", "")));
  
  auto sc = pt_dev.get<std::string>("platform.current_shell.sc_version", "");
  bool same_sc = ((sc.empty()) || (dsa.bmcVer.empty()) || 
                  (dsa.bmcVer == sc) || (sc.find("FIXED") != std::string::npos));

  // Always update Arista devices
  auto vendor = xrt_core::device_query<xrt_core::query::pcie_vendor>(workingDevice);
  if (vendor == ARISTA_ID)
    same_shell = false;

  if (XBU::getForce()) {
    same_shell = false;
    same_sc = false;
  }

  if (!same_shell || !same_sc) {
    if(!dsa.hasFlashImage)
      throw xrt_core::error("Flash image is not available");

    boardsToUpdate.push_back(std::make_pair(workingDevice->get_device_id(), dsa));
  }
  
  // Is there anything to flash
  if (boardsToUpdate.empty() == true) {
    std::cout << "\nDevice is up-to-date.  No flashing to performed.\n";
    return;
  }

  // Update the ptree with the status
  pt_dev.put("platform.status.shell", same_shell);
  pt_dev.put("platform.status.sc", same_sc);

  //report status of the device
  report_status(workingDevice, dsa.name, pt_dev);

  // Continue to flash whatever we have collected in boardsToUpdate.
  bool needreboot = false;
  bool need_warm_reboot = false;
  std::stringstream report_stream;

  // Prompt user about what boards will be updated and ask for permission.
  if(!XBU::can_proceed(XBU::getForce()))
    return;

  // Perform DSA and BMC updating
  std::stringstream error_stream;
  for (auto& p : boardsToUpdate) {
    try {
      std::cout << std::endl;
      // 1) Flash the Satellite Controller image
      if (update_sc(p.first, p.second) == true)  {
        report_stream << boost::format("  [%s] : Successfully flashed the Satellite Controller (SC) image\n") % getBDF(p.first);
        need_warm_reboot = true;
      } else
        report_stream << boost::format("  [%s] : Satellite Controller (SC) is either up-to-date, fixed, or not installed. No actions taken.\n") % getBDF(p.first);

      // 2) Flash shell image
      if (update_shell(p.first, p.second) == true)  {
        report_stream << boost::format("  [%s] : Successfully flashed the base (e.g., shell) image\n") % getBDF(p.first);
        needreboot = true;
      } else
        report_stream << boost::format("  [%s] : Base (e.g., shell) image is up-to-date.  No actions taken.\n") % getBDF(p.first);
      } catch (const xrt_core::error& e) {
        error_stream << boost::format("ERROR: %s\n") % e.what();
      }
  }

  std::cout << "----------------------------------------------------\n";
  std::cout << "Report\n";
  std::cout << report_stream.str();


  if (error_stream.str().empty()) {
    std::cout << "\nDevice flashed successfully.\n"; 
  } else {
    std::cout << "\nDevice flashing encountered errors:\n";
    std::cerr << error_stream.str();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  if (needreboot) {
    std::cout << "****************************************************\n";
    std::cout << "Cold reboot machine to load the new image on device.\n";
    std::cout << "****************************************************\n";
  } else if (need_warm_reboot) {
    std::cout << "******************************************************************\n";
    std::cout << "Warm reboot is required to recognize new SC image on the device.\n";
    std::cout << "******************************************************************\\n";
  }
}

static void
program_plp(const xrt_core::device* dev, const std::string& partition)
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

  try {
    xrt_core::program_plp(dev, buffer, XBU::getForce());
  } 
  catch(xrt_core::error& e) {
    std::cout << "ERROR: " << e.what() << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }
  std::cout << "Programmed shell successfully" << std::endl;
}

static std::vector<std::string>
find_flash_image_paths(const std::vector<std::string> image_list)
{
  std::vector<std::string> path_list;
  auto installedShells = firmwareImage::getIntalledDSAs();
  
  //iterates over installed shells
  //checks the vbnv against the vnbv passed in by the user
  auto get_shell_file = [installedShells] (std::string shell_name) {
    std::string path;
    int multiple_shells = 0;
    
    for(auto const& shell : installedShells) {
      if(shell_name.compare(shell.name) == 0) {
        multiple_shells++;
        path = shell.file;
      }
    }

    //error-handling
    if(multiple_shells == 0) 
      throw xrt_core::error("Specified base not found on the system");
    
    //if multiple shells with the same vbnv are installed on the system, we don't want to 
    //blindly update the device. in this case, the user needs to specify the complete path
    if(multiple_shells > 1) 
      throw xrt_core::error("Specified base matched mutiple installed bases. Please specify the full path.");

    return path;
  };

  for(const auto& img : image_list) {
    // check if the passed in image is absolute path
    if(boost::filesystem::exists(boost::filesystem::path(img))) {
      path_list.push_back(img);
    }
    else { //search installed shells and find the image path by checking the vbnv
      path_list.push_back(get_shell_file(img));
    }
  }
  return path_list;
}

}
//end anonymous namespace

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdProgram::SubCmdProgram(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("program", 
             "Update image(s) for a given device")
{
  const std::string longDescription = "Updates the image(s) for a given device.";
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
  std::string xclbin = "";
  std::string flashType = "";
  std::vector<std::string> image;
  bool revertToGolden = false;
  bool help = false;

  po::options_description commonOptions("Common Options");  
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(device)>(&device)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("shell,s", boost::program_options::value<decltype(plp)>(&plp), "The partition to be loaded.  Valid values:\n"
                                                                      "  Name (and path) of the partition.")

    // TODO: Auto update the 'base' values
    ("base,b", boost::program_options::value<decltype(update)>(&update)->implicit_value("all"), "Update the persistent images and/or the Satellite controller (SC) firmware image."/*  Value values:\n"
                                                                         "  ALL   - All images will be updated"
                                                                         "  FLASH - Flash image\n"
                                                                         "  SC    - Satellite controller"*/)
    ("user,u", boost::program_options::value<decltype(xclbin)>(&xclbin), "The xclbin to be loaded.  Valid values:\n"
                                                                      "  Name (and path) of the xclbin.")
    ("image", boost::program_options::value<decltype(image)>(&image)->multitoken(), "Specifies an image to use used to update the persistent device.  Value values:\n"
                                                                    "  Name (and path) to the mcs image on disk\n"
                                                                    "  Name (and path) to the xsabin image on disk")
    ("revert-to-golden", boost::program_options::bool_switch(&revertToGolden), "Resets the FPGA PROM back to the factory image. Note: The Satellite Controller will not be reverted for a golden image does not exist.")
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
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(boost::str(boost::format("  Base: %s") % update));

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
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;

  xrt_core::device_collection deviceCollection;
  for (const auto & deviceName : device) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  XBU::collect_devices(deviceNames, false /*inUserDomain*/, deviceCollection);

  // Enforce 1 device specification
  if (deviceCollection.size() > 1) {
    std::cerr << "\nERROR: Multiple device programming is not supported. Please specify a single device using --device option\n\n";
    std::cout << "List of available devices:" << std::endl;

    boost::property_tree::ptree available_devices = XBU::get_available_devices(false);
    for(auto& kd : available_devices) {
      boost::property_tree::ptree& _dev = kd.second;
      std::cout << boost::format("  [%s] : %s\n") % _dev.get<std::string>("bdf") % _dev.get<std::string>("vbnv");
    }

    std::cout << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Make sure we have at least 1 device
  if (deviceCollection.size() == 0) 
    throw std::runtime_error("No devices found.");

  // Get the device
  auto & workingDevice = deviceCollection[0];

  // TODO: Added mutually exclusive code for image, update, and revert-to-golden action.

  if (!image.empty()) {

    // image is a sub-option of update
    if (update.empty())
      throw xrt_core::error("Usage: xbmgmt program --device='0000:00:00.0' --base --image='/path/to/flash_image' OR shell_name");

    // We support up to 2 flash images 
    if (image.size() == 1)
      auto_flash(workingDevice, image.front());
    
    if (image.size() == 2) {
      std::cout << "CAUTION! Force flashing the platform on the device without any checks." <<
                   "Please make sure that the correct information is passed in." << std::endl;
      auto image_paths = find_flash_image_paths(image);
      update_shell(workingDevice->get_device_id(), flashType, image_paths.front(), (image_paths.size() == 2 ? image_paths[1]: ""));
    }
    
    if (image.size() > 2)
      throw xrt_core::error("Please specify either 1 or 2 flash images");
    return;
  }

  if (!update.empty()) {
    XBU::verbose("Sub command: --base");
    XBUtilities::sudo_or_throw("Root privileges are required to update the devices flash image");
    if (update.compare("all") == 0)
      // Note: To get around a bug in the SC flashing code base,
      //       auto_flash will clear the collection. This code need to be refactored and clean up.
      auto_flash( workingDevice );
    else {
      if (update.compare("flash") == 0)
        throw xrt_core::error("Platform only update is not supported");

      if (update.compare("sc") == 0)
        throw xrt_core::error("SC only update is not supported");
       
      throw xrt_core::error("Please specify a valid value");
    }
    return;
  }
  
  // -- process "revert-to-golden" option ---------------------------------------
  if(revertToGolden) {
    XBU::verbose("Sub command: --revert-to-golden");
    bool has_reset = false;

    std::vector<Flasher> flasher_list;
    for (const auto & dev : deviceCollection) {
      //collect information of all the devices that will be reset
      Flasher flasher(dev->get_device_id());
      if(!flasher.isValid()) {
        xrt_core::error(boost::str(boost::format("%d is an invalid index") % dev->get_device_id()));
        continue;
      }
      std::cout << boost::format("%-8s : %s %s %s \n") % "INFO" % "Resetting device [" 
        % flasher.sGetDBDF() % "] back to factory mode.";
      flasher_list.push_back(flasher);
    }
    
    XBUtilities::sudo_or_throw("Root privileges are required to revert the device to its golden flash image");

    //ask user's permission
    if(!XBU::can_proceed(XBU::getForce()))
      throw xrt_core::error(std::errc::operation_canceled);
    
    for(auto& f : flasher_list) {
      if (!f.upgradeFirmware("", nullptr, nullptr)) {
        std::cout << boost::format("%-8s : %s %s %s\n") % "INFO" % "Shell on [" % f.sGetDBDF() % "] is reset successfully." ;
        has_reset = true;
      }
    }

    if (!has_reset)
      return;

    std::cout << "****************************************************\n";
    std::cout << "Cold reboot machine to load the new image on device.\n";
    std::cout << "****************************************************\n";

    return;
  }

  // -- process "plp" option ---------------------------------------
  if(!plp.empty()) {
    XBU::verbose(boost::str(boost::format("  shell: %s") % plp));
    //only 1 device and name
    if(deviceCollection.size() > 1)
      throw xrt_core::error("Please specify a single device");

    auto dev = deviceCollection.front();

    Flasher flasher(dev->get_device_id());
    if(!flasher.isValid())
      throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % dev->get_device_id()));

    if(xrt_core::device_query<xrt_core::query::interface_uuids>(dev).empty())
      throw xrt_core::error("Can not get BLP interface uuid. Please make sure corresponding BLP package is installed.");

    // Check if file exists
    if (!boost::filesystem::exists(plp))
      throw xrt_core::error("File not found. Please specify the correct path");

    DSAInfo dsa(plp);
    //TO_DO: add a report for plp before asking permission to proceed. Replace following 2 lines
    std::cout << "Programming shell on device [" << flasher.sGetDBDF() << "]..." << std::endl;
    std::cout << "Partition file: " << dsa.file << std::endl;
    
    for (const auto& uuid : dsa.uuids) {

      //check if plp is compatible with the installed blp
      if (xrt_core::device_query<xrt_core::query::interface_uuids>(dev).front().compare(uuid) == 0) {
        XBUtilities::sudo_or_throw("Root privileges are required to load the PLP image");
        program_plp(dev.get(), dsa.file);
        return;
      }
    }

    // Fall through error
    throw xrt_core::error("uuid does not match BLP");
  }

  // -- process "user" option ---------------------------------------
  if(!xclbin.empty()) {
    XBU::verbose(boost::str(boost::format("  xclbin: %s") % xclbin));
    XBU::sudo_or_throw("Root privileges are required to download xclbin");
    //only 1 device and name
    if(deviceCollection.size() > 1)
      throw xrt_core::error("Please specify a single device");
    auto dev = deviceCollection.front();

    std::ifstream stream(xclbin, std::ios::binary);
    if (!stream)
      throw xrt_core::error(boost::str(boost::format("Could not open %s for reading") % xclbin));

    stream.seekg(0,stream.end);
    ssize_t size = stream.tellg();
    stream.seekg(0,stream.beg);

    std::vector<char> xclbin_buffer(size);
    stream.read(xclbin_buffer.data(), size);
    
    auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(dev));
    std::cout << "Downloading xclbin on device [" << bdf << "]..." << std::endl;
    try {
      dev->xclmgmt_load_xclbin(xclbin_buffer.data());
    } catch (xrt_core::error& e) {
      std::cout << "ERROR: " << e.what() << std::endl;
      throw xrt_core::error(std::errc::operation_canceled);
    }
    std::cout << boost::format("INFO: Successfully downloaded xclbin \n") << std::endl;
    return;
}

  std::cout << "\nERROR: Missing flash operation.  No action taken.\n\n";
  printHelp(commonOptions, hiddenOptions);
  throw xrt_core::error(std::errc::operation_canceled);
}
