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
#include "tools/common/ProgressBar.h"
namespace XBU = XBUtilities;

#include "xrt.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/query_requests.h"
#include "core/common/message.h"
#include "core/common/error.h"
#include "flash/flasher.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <ctime>
#include <locale>

#ifdef _WIN32
#pragma warning(disable : 4996) //std::asctime
#endif


// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {
/*
 * Update shell on the board
 */
static void 
update_shell(uint16_t index, const std::string& flashType,
    const std::string& primary, const std::string& secondary)
{
  std::unique_ptr<firmwareImage> pri;
  std::unique_ptr<firmwareImage> sec;

  if (!flashType.empty()) {
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", 
        "Overriding flash mode is not recommended.\nYou may damage your card with this option.");
  }

  Flasher flasher(index);
  if(!flasher.isValid())
    throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % index));

  if (primary.empty())
    throw xrt_core::error("Shell not specified");

  pri = std::make_unique<firmwareImage>(primary.c_str(), MCS_FIRMWARE_PRIMARY);
  if (pri->fail())
    throw xrt_core::error(boost::str(boost::format("Failed to read %s") % primary));
  if (!secondary.empty()) {
    sec = std::make_unique<firmwareImage>(secondary.c_str(),
      MCS_FIRMWARE_SECONDARY);
    if (sec->fail())
      sec = nullptr;
  }

  flasher.upgradeFirmware(flashType, pri.get(), sec.get());
  std::cout << boost::format("%-8s : %s \n") % "INFO" % "Shell is updated succesfully.";
}

/*
 * Update SC firmware on the board
 */
static void 
update_SC(uint16_t index, const std::string& file)
{
  Flasher flasher(index);
  if(!flasher.isValid())
    throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % index));

  std::unique_ptr<firmwareImage> bmc =
    std::make_unique<firmwareImage>(file.c_str(), BMC_FIRMWARE);
  if (bmc->fail())
    throw xrt_core::error(boost::str(boost::format("Failed to read %s") % file));

  flasher.upgradeBMCFirmware(bmc.get());
}

/* 
 * Helper function for header info
 */
static std::string 
file_size(const char* file) 
{
  std::ifstream in(file, std::ifstream::ate | std::ifstream::binary);
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
get_file_timestamp(const char* file) 
{
  boost::filesystem::path p(file);
	if (!boost::filesystem::exists(p)) {
		throw xrt_core::error("Invalid platform path.");
	}
  std::time_t ftime = boost::filesystem::last_write_time(boost::filesystem::path(file));
  return std::string(std::asctime(std::localtime(&ftime)));
}

/*
 * Header info
 */
static void
status_report(const std::string& bdf, const DSAInfo& currentDSA, const DSAInfo& candidate)
{
  std::cout << boost::format("%s : %d\n") % "Device BDF" % bdf;
  std::cout << "Current Configuration\n";

  std::cout << boost::format("  %-20s : %s\n") % "Platform" % currentDSA.name;
  std::cout << boost::format("  %-20s : %s\n") % "SC Version" % currentDSA.bmcVer;
  std::cout << boost::format("  %-20s : 0x%x\n") % "Platform ID" % currentDSA.timestamp;

  std::cout << "\nIncoming Configuration\n";
  std::pair <std::string, std::string> s = deployment_path_and_filename(candidate.file);
  std::cout << boost::format("  %-20s : %s\n") % "Deployment File" % s.first;
  std::cout << boost::format("  %-20s : %s\n") % "Deployment Directory" % s.second;
  std::cout << boost::format("  %-20s : %s\n") % "Size" % file_size(candidate.file.c_str());
  std::cout << boost::format("  %-20s : %s\n\n") % "Timestamp" % get_file_timestamp(candidate.file.c_str());

  std::cout << boost::format("  %-20s : %s\n") % "Platform" % candidate.name;
  std::cout << boost::format("  %-20s : %s\n") % "SC Version" % candidate.bmcVer;
  std::cout << boost::format("  %-20s : 0x%x\n\n") % "Platform ID" % candidate.timestamp;
}

/* 
 * Find the correct shell to be flashed on the board
 * Helper method for auto_flash
 */
static DSAInfo 
selectShell(uint16_t idx, const std::string& dsa, const std::string& id)
{
  uint16_t candidateDSAIndex = std::numeric_limits<uint16_t>::max();
  Flasher flasher(idx);
  if(!flasher.isValid())
    throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % idx));

  std::vector<DSAInfo> installedDSA = flasher.getInstalledDSA();

  // Find candidate DSA from installed DSA list.
  if (dsa.empty()) {
    if (installedDSA.empty())
      throw xrt_core::error("No platform is installed.");
    if (installedDSA.size() > 1)
      throw xrt_core::error("Multiple platforms are installed.");
    candidateDSAIndex = 0;
  } else {
    for (uint16_t i = 0; i < installedDSA.size(); i++) {
      const DSAInfo& idsa = installedDSA[i];
      if (dsa != idsa.name)
        continue;
      if (!id.empty() && !idsa.matchId(id))
        continue;
      if (candidateDSAIndex != std::numeric_limits<uint16_t>::max()) {
        return DSAInfo("");
      }
      candidateDSAIndex = i;
    }
  }
  if (candidateDSAIndex == std::numeric_limits<uint16_t>::max())
    throw xrt_core::error(boost::str(boost::format
      ("Failed to flash device[%s]: Specified platform is not applicable") % flasher.sGetDBDF()));

  DSAInfo& candidate = installedDSA[candidateDSAIndex];
  bool same_dsa = false;
  bool same_bmc = false;
  DSAInfo currentDSA = flasher.getOnBoardDSA();
  if (!currentDSA.name.empty()) {
    same_dsa = ((candidate.name == currentDSA.name) &&
      (candidate.matchId(currentDSA)));
    same_bmc = ((currentDSA.bmcVer.empty()) ||
      (candidate.bmcVer == currentDSA.bmcVer));
  }
  if (same_dsa && same_bmc) {
    return DSAInfo("");
  }

  status_report(flasher.sGetDBDF(), currentDSA, candidate);

  //decide and display which actions to perform
  std::cout << "----------------------------------------------------\n";
  std::cout << "Actions to perform:\n";
  if(!same_dsa)
    std::cout << "  -Program flash image\n";
  if(!same_bmc)
    std::cout << "  -Program SC image\n";
  std::cout << "----------------------------------------------------\n";

  return candidate;
}

/* 
 * Confirm with the user
 * Helper method for auto_flash
 */
static bool 
canProceed()
{
  bool proceed = false;
  std::string input;

  std::cout << "Are you sure you wish to proceed? [Y/n]: ";
  std::getline( std::cin, input );

  // Ugh, the std::transform() produces windows compiler warnings due to 
  // conversions from 'int' to 'char' in the algorithm header file
  boost::algorithm::to_lower(input);
  //std::transform( input.begin(), input.end(), input.begin(), [](unsigned char c){ return std::tolower(c); });
  //std::transform( input.begin(), input.end(), input.begin(), ::tolower);

  // proceeds for "y", "Y" and no input
  proceed = ((input.compare("y") == 0) || input.empty());
  if (!proceed)
    std::cout << "Action canceled." << std::endl;
  return proceed;
}

/* 
 * Flash shell and sc firmware
 * Helper method for auto_flash
 */
static int 
updateShellAndSC(uint16_t boardIdx, DSAInfo& candidate, bool& reboot)
{
  reboot = false;

  Flasher flasher(boardIdx);

  bool same_dsa = false;
  bool same_bmc = false;
  DSAInfo current = flasher.getOnBoardDSA();
  if (!current.name.empty()) {
    same_dsa = (candidate.name == current.name &&
      candidate.matchId(current));
    same_bmc = (candidate.bmcVer == current.bmcVer);
  }
  if (same_dsa && same_bmc) {
    std::cout << "update not needed" << std::endl;
    return 0;
  }

  if (!same_bmc) {
    std::cout << "Updating SC firmware on card[" << flasher.sGetDBDF() <<
      "]" << std::endl;
    auto ret = 0;
    update_SC(boardIdx, candidate.file);
    if (ret != 0) {
      std::cout << "WARNING: Failed to update SC firmware on card ["
        << flasher.sGetDBDF() << "]" << std::endl;
    }
  }

  if (!same_dsa) {
    std::cout << "Updating shell on card[" << flasher.sGetDBDF() <<
      "]" << std::endl;
    auto ret = 0;
    update_shell(boardIdx, "", candidate.file,
      candidate.file);
    if (ret != 0) {
      std::cout << "ERROR: Failed to update shell on card["
        << flasher.sGetDBDF() << "]" << std::endl;
    } else {
      reboot = true;
    }
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

static void
validate_dsa_timestamp(std::string& name, std::string& id)
{
  if (!name.empty()) {
    bool foundDSA = false;
    bool multiDSA = false;
    auto installedDSAs = firmwareImage::getIntalledDSAs();
    for (DSAInfo& dsa : installedDSAs) {
      if (name == dsa.name &&
        (id.empty() || dsa.matchId(id))) {
          multiDSA = multiDSA || foundDSA;
          foundDSA = true;
      }
    }
    if (!foundDSA)
      throw xrt_core::error("Specified shell not found");
    if (multiDSA)
      throw xrt_core::error("Specified shell matched multiple installed shells");
  }
}

/* 
 * Update shell and sc firmware on the card automatically
 */
static void 
auto_flash(uint16_t index, std::string& name,
    std::string& id, bool force) 
{
  std::vector<uint16_t> boardsToCheck;
  std::vector<std::pair<uint16_t, DSAInfo>> boardsToUpdate;

  // Sanity check input dsa and timestamp.
  validate_dsa_timestamp(name, id);

  // Collect all indexes of boards need checking
  auto total = xrt_core::get_total_devices(false).first;
  if (index == std::numeric_limits<uint16_t>::max()) {
    for(uint16_t i = 0; i < total; i++)
      boardsToCheck.push_back(i);
  } else {
    if (index < total)
      boardsToCheck.push_back(index);
  }
  if (boardsToCheck.empty())
    throw xrt_core::error("Card not found");

  // Collect all indexes of boards need updating
  for (uint16_t i : boardsToCheck) {
    DSAInfo dsa = selectShell(i, name, id);
    if (dsa.hasFlashImage)
      boardsToUpdate.push_back(std::make_pair(i, dsa));
  }

  // Continue to flash whatever we have collected in boardsToUpdate.
  uint16_t success = 0;
  bool needreboot = false;
  std::stringstream report_status;
  if (!boardsToUpdate.empty()) {

    // Prompt user about what boards will be updated and ask for permission.
    if(!force && !canProceed())
      return;

    // Perform DSA and BMC updating
    for (auto& p : boardsToUpdate) {
      bool reboot;
      std::cout << std::endl;
      if (updateShellAndSC(p.first, p.second, reboot) == 0) {
        report_status << "  Successfully flashed card[" << getBDF(p.first) << "]\n";
        success++;
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
  XBU::verbose("SubCommand: examine");

  XBU::verbose("Option(s):");
  for (auto & aString : _options) {
    std::string msg = "   ";
    msg += aString;
    XBU::verbose(msg);
  }

  // -- Retrieve and parse the subcommand options -----------------------------
  std::string device = "";
  std::vector<uint16_t> device_indices = {0};
  std::string plp = "";
  std::string update = "";
  std::string image = "";
  bool revertToGolden = false;
  bool test_mode = false;
  bool help = false;

  po::options_description queryDesc("Options");  // Note: Boost will add the colon.
  queryDesc.add_options()
    ("device,d", boost::program_options::value<decltype(device)>(&device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.  A value of 'all' (default) indicates that every found device should be examined.")
    ("plp", boost::program_options::value<decltype(plp)>(&plp), "The partition to be loaded.  Valid values:\n"
                                                                "  Name (and path) of the partiaion.\n"
                                                                "  Parition's UUID")
    ("update", boost::program_options::value<decltype(update)>(&update)->implicit_value("all"), "Update the persistent images.  Value values:\n"
                                                                         "  ALL   - All images will be updated\n"
                                                                         "  FLASH - Flash image\n"
                                                                         "  SC    - Satellite controller\n")
    ("image", boost::program_options::value<decltype(image)>(&image), "Specifies an image to use used to update the persistent device(s).  Value values:\n"
                                                                      "  Name of the device\n"
                                                                      "  Name (and path) to the mcs image on disk\n"
                                                                      "  Name (and path) to the xsabin image on disk")
    ("revert-to-golden", boost::program_options::bool_switch(&revertToGolden), "Resets the FPGA PROM back to the factory image.  Note: This currently only applies to the flash image.")
    ("test_mode", boost::program_options::bool_switch(&test_mode), "Animate flash progress bar")
    ("help,h", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(queryDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(queryDesc);
    throw; // Re-throw exception
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(queryDesc);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(boost::str(boost::format("  Card: %s") % device));
  XBU::verbose(boost::str(boost::format("  Update: %s") % update));
  // Is valid BDF value valid

  if (test_mode) {
    std::cout << "\n>>> TEST MODE <<<\n"
              << "Simulating programming the flash device with a failure.\n\n"
              << "Flash image: xilinx_u250_xdma_201830_1.mcs\n"
              << "  Directory: /lib/firmware/xilinx\n"
              << "  File Size: 134,401,924 bytes\n"
              << " Time Stamp: Feb 1, 2020 08:07\n\n";
    //standard use case
    XBU::ProgressBar flash("Erasing flash", 8, XBU::is_esc_enabled(), std::cout);
    for (int i = 1; i <= 8; i++) {
      if (i != 8) {
        for (int fastLoop = 0; (fastLoop <= 10); ++fastLoop) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          flash.update(i);
        }
      } else {
        flash.update(i);
      }
	  }
    flash.finish(true, "Flash erased");

    //failure case
    XBU::ProgressBar fail_flash("Programming flash", 10, XBU::is_esc_enabled(), std::cout);
    for (int i = 1; i <= 8; i++) {
		  std::this_thread::sleep_for(std::chrono::milliseconds(500));
		  fail_flash.update(i);
	  }

    for (int i = 0; i < 20; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      fail_flash.update(8);
    }
    fail_flash.finish(false, "An error has occurred while programming the flash image");

    //Add gtest to test error when iteration > max_iter
  }

  // get all device IDs to be processed
  if (!device.empty()) { 
    XBU::verbose("Sub command : --device");
    using tokenizer = boost::tokenizer< boost::char_separator<char> >;
    boost::char_separator<char> sep(", ");
    tokenizer tokens(device, sep);
    
    for (auto tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
    	uint16_t idx = xrt_core::bdf2index(*tok_iter);
      device_indices.push_back(idx);
    }
  }

  if (!update.empty()) {
    XBU::verbose("Sub command: --update");
    // deal with 
    //   - list of cards
    // currently doing 1st card if not specified
    std::string empty = "";
    if(update.compare("all") == 0)
      auto_flash(device_indices[0], empty, empty, false);
    else if(update.compare("flash") == 0)
      std::cout << "TODO: implement platform only update\n";
    else if(update.compare("sc") == 0)
      std::cout << "TODO: implement SC only update\n";
    else 
      throw xrt_core::error("Please specify a valid value");
    return;
  }
}
