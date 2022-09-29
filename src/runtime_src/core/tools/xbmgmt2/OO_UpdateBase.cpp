// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
#include "OO_UpdateBase.h"

// XRT - Include Files
#include "core/common/info_vmr.h"
#include "core/common/message.h"
#include "flash/flasher.h"
#include "ReportPlatform.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/ProgressBar.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
// =============================================================================

// ----- H E L P E R M E T H O D S ------------------------------------------


// ----- C L A S S   M E T H O D S -------------------------------------------

OO_UpdateBase::OO_UpdateBase(const std::string &_longName, const std::string &_shortName, bool _isHidden )
    : OptionOptions(_longName,
                    _shortName,
                    "Update base partition",
                    boost::program_options::value<decltype(update)>(&update)->implicit_value("all")->required(),
                    "Update the persistent images and/or the Satellite controller (SC) firmware image.  Valid values:\n"
                      "  ALL   - All images will be updated\n"
                      "  SHELL - Platform image\n"
                      "  SC    - Satellite controller (Warning: Damage could occur to the device)\n"
                      "  NO-BACKUP   - Backup boot remains unchanged",
                    _isHidden)
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("image", boost::program_options::value<decltype(image)>(&image)->multitoken(),  "Specifies an image to use used to update the persistent device.  Valid values:\n"
                                                                    "  Name (and path) to the mcs image on disk\n"
                                                                    "  Name (and path) to the xsabin image on disk")
    ("help", po::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_optionsHidden.add_options()
    ("flash-type", boost::program_options::value<decltype(flashType)>(&flashType),
      "Overrides the flash mode. Use with caution.  Valid values:\n"
      "  ospi\n"
      "  ospi_versal")
    ;
}

// Create a list of images that are known to exist based on given names and paths
static std::vector<std::string>
find_flash_image_paths(const std::vector<std::string>& image_list)
{
  std::vector<std::string> path_list;
  auto installedShells = firmwareImage::getIntalledDSAs();

  for (const auto& img : image_list) {
    // Check if the passed in image is absolute path
    if (boost::filesystem::is_regular_file(img)){
      if (boost::filesystem::extension(img).compare(".xsabin") != 0) {
        std::cout << "Warning: Non-xsabin file detected. Development usage, this may damage the card\n";
        if (!XBU::can_proceed(XBU::getForce()))
          throw xrt_core::error(std::errc::operation_canceled);
      }
      path_list.push_back(img);
    }
    // Search through the installed shells and get the complete path
    else {
       // Checks installed shell names against the shell name passed in by the user
      std::string img_path;
      for (auto const& shell : installedShells) {
        if (img.compare(shell.name) == 0) {
          // Only set the image path on the first shell match
          if (img_path.empty())
            img_path = shell.file;
          // If multiple shells with the same name are installed on the system, we don't want to
          // blindly update the device. In this case, the user needs to specify the complete path
          else
            throw xrt_core::error("Specified base matched mutiple installed bases. Please specify the full path.");
        }
      }

      if (img_path.empty())
        throw xrt_core::error("Specified base not found on the system");

      path_list.push_back(img_path);
    }
  }
  return path_list;
}

/**
 * @brief Update shell on the board for manual flash
 *
 * @param[in] index The index of the board to be flashed
 * @param[in] image_paths The images to flash onto the board
 * @param[in] flash_type The format in which the board will be flashed. Leave
 *                       blank to use the boards default flashing mode
 */
static void
update_shell(unsigned int index, std::map<std::string, std::string>& image_paths, Flasher::E_FlasherType flash_type)
{
  Flasher flasher(index);
  if (!flasher.isValid())
    throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % index));

  if (image_paths.empty())
    throw xrt_core::error("No image specified.\n Usage: xbmgmt program --device='0000:00:00.0' --base [all|sc|shell]"
                            " --image=['/path/to/flash_image'|'shell name']");

  auto pri = std::make_unique<firmwareImage>(image_paths["primary"], MCS_FIRMWARE_PRIMARY);
  if (pri->fail())
    throw xrt_core::error(boost::str(boost::format("Failed to read %s") % image_paths["primary"]));

  auto stripped = std::make_unique<firmwareImage>(image_paths["primary"], STRIPPED_FIRMWARE);
  if (stripped->fail())
    stripped = nullptr;

  std::unique_ptr<firmwareImage> sec;
  if (image_paths.size() > 1) {
    sec = std::make_unique<firmwareImage>(image_paths["secondary"], MCS_FIRMWARE_SECONDARY);
    if (sec->fail())
      throw xrt_core::error(boost::str(boost::format("Failed to read %s") % image_paths["secondary"]));
  }

  if (flasher.upgradeFirmware(flash_type, pri.get(), sec.get(), stripped.get()) != 0)
    throw xrt_core::error("Failed to update base");

  std::cout << boost::format("%-8s : %s \n") % "INFO" % "Base flash image has been programmed successfully.";
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

//versal flow to flash sc
static void
update_versal_SC(std::shared_ptr<xrt_core::device> dev)
{
  std::thread t;
  std::atomic<bool> done(false);
  std::shared_ptr<XBU::ProgressBar> progress_reporter;
  try {
    const uint32_t val = xrt_core::query::program_sc::value_type(1);
    // timeout for xgq is 300 seconds
    const unsigned int max_duration = 300;
    progress_reporter = std::make_shared<XBU::ProgressBar>("Programming SC", 
                                max_duration, true /*batch mode for dots*/, std::cout); 
    progress_reporter.get()->setPrintPercentBatch(false);
    // Print progress while sc is flashed
    t = std::thread([&progress_reporter, &done]() {
      unsigned int counter = 0;
      while ((counter < progress_reporter.get()->getMaxIterations()) && !done) {
        progress_reporter.get()->update(counter++);
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    });
    xrt_core::device_update<xrt_core::query::program_sc>(dev.get(), val);
    done = true;
    progress_reporter.get()->finish(true, "SC firmware image has been programmed successfully.");
    t.join();
  }
  catch (const xrt_core::query::sysfs_error& e) {
    done = true;
    progress_reporter.get()->finish(false, "Failed to update SC flash image.");
    t.join();
    throw xrt_core::error(std::string("Error accessing sysfs entry : ") + e.what());
  }
}

// Update SC firmware on the board
static void
update_SC(unsigned int  index, const std::string& file)
{
  Flasher flasher(index);

  if (!flasher.isValid())
    throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % index));

  auto dev = xrt_core::get_mgmtpf_device(index);

  bool is_versal = xrt_core::device_query<xrt_core::query::is_versal>(dev);
  if (is_versal) {
    update_versal_SC(dev);
    return;
  }

  //if factory image, update SC
  auto is_mfg = xrt_core::device_query<xrt_core::query::is_mfg>(dev);
  if (is_mfg) {
    std::unique_ptr<firmwareImage> bmc = std::make_unique<firmwareImage>(file, BMC_FIRMWARE);
    if (bmc->fail())
      throw xrt_core::error(boost::str(boost::format("Failed to read %s") % file));

    if (flasher.upgradeBMCFirmware(bmc.get()) != 0)
      throw xrt_core::error("Failed to update SC flash image");
    return;
  }

  // If SC is fixed, stop flashing immediately
  if (is_SC_fixed(index))
    throw xrt_core::error("SC is fixed, unable to flash image.");

  // Mgmt pf needs to shutdown so that the board doesn't brick
  try {
    dev->device_shutdown();
  }
  catch (const xrt_core::error& e) {
    throw xrt_core::error(std::string("Only proceed with SC update if all user applications for the target card(s) are stopped. ") + e.what());
  }

  std::unique_ptr<firmwareImage> bmc = std::make_unique<firmwareImage>(file, BMC_FIRMWARE);

  if (bmc->fail())
    throw xrt_core::error(boost::str(boost::format("Failed to read %s") % file));

  if (flasher.upgradeBMCFirmware(bmc.get()) != 0)
    throw xrt_core::error("Failed to update SC flash image");

  // Bring back mgmt pf
  try {
    dev->device_online();
  }
  catch (const xrt_core::error& e) {
    throw xrt_core::error(e.what() + std::string(" Please warm reboot."));
  }

  std::cout << boost::format("%-8s : %s \n\n") % "INFO" % "SC firmware image has been programmed successfully.";
}

// Helper function for header info
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

// Helper function for header info
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

// Helper function for header info
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
  for (auto& image : available_shells) {
    if ((image.second.get<std::string>("vbnv")).compare(vbnv) == 0) {
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
report_status(const std::string& vbnv, boost::property_tree::ptree& pt_device)
{
  std::cout << "----------------------------------------------------\n";
  pretty_print_platform_info(pt_device, vbnv);
  std::cout << "----------------------------------------------------\n";

  std::stringstream action_list;

  if (!pt_device.get<bool>("platform.status.shell"))
    action_list << boost::format("  [%s] : Program base (FLASH) image\n") % pt_device.get<std::string>("platform.bdf");

  if (!pt_device.get<bool>("platform.status.sc") && !pt_device.get<bool>("platform.status.is_factory") && !pt_device.get<bool>("platform.status.is_recovery"))
    action_list << boost::format("  [%s] : Program Satellite Controller (SC) image\n") % pt_device.get<std::string>("platform.bdf");

  if (!action_list.str().empty()) {
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
  if (current.bmc_ver().empty() || candidate.bmc_ver().empty()) {
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
  boost::format programFmt("[%s] : %s\n");
  std::cout << programFmt % flasher.sGetDBDF() % "Updating Satellite Controller (SC) firmware flash image";
  update_SC(boardIdx, candidate.file);
  std::cout << std::endl;

  return true;
}



// Flash shell and sc firmware
// Helper method for auto_flash
static bool
update_shell(unsigned int boardIdx, DSAInfo& candidate, Flasher::E_FlasherType flash_type)
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
  std::map<std::string, std::string> validated_image = {{"primary", candidate.file}};
  std::unique_ptr<firmwareImage> sec = std::make_unique<firmwareImage>(candidate.file, MCS_FIRMWARE_SECONDARY);
  if (sec->good())
    validated_image["secondary"] = candidate.file;

  update_shell(boardIdx, validated_image, flash_type);
  return true;
}

static void
update_default_only(xrt_core::device* device, bool value)
{
  try {
    // get boot on backup from vmr_status sysfs node
    boost::property_tree::ptree pt_empty;
    const auto pt = xrt_core::vmr::vmr_info(device).get_child("vmr", pt_empty);
    for (const auto& ks : pt) {
      const boost::property_tree::ptree& vmr_stat = ks.second;
      if (boost::iequals(vmr_stat.get<std::string>("label"), "Boot on default")) {
        // if backup is booted, then do not proceed
        if (std::stoi(vmr_stat.get<std::string>("value")) != 1) {
          std::cout << "Backup image booted. Action will be performed only on default image.\n";
        }
        break;
      }
    }

    uint32_t val = xrt_core::query::flush_default_only::value_type(value);
    xrt_core::device_update<xrt_core::query::flush_default_only>(device, val);
  }
  catch (const xrt_core::query::exception&) {
    // only available for versal devices
  }
}

// Update shell and sc firmware on the device automatically
// Refactor code to support only 1 device.
static void
auto_flash(std::shared_ptr<xrt_core::device> & device, Flasher::E_FlasherType flashType, const std::string& image = "")
{
  // Get platform information
  boost::property_tree::ptree pt;
  boost::property_tree::ptree ptDevice;
  auto rep = std::make_unique<ReportPlatform>();
  rep->getPropertyTreeInternal(device.get(), ptDevice);
  pt.push_back(std::make_pair(std::to_string(device->get_device_id()), ptDevice));

  // Collect all indexes of boards need updating
  std::vector<std::pair<unsigned int , DSAInfo>> boardsToUpdate;

  std::string image_path = image; // Set default image path
  if (image_path.empty()) {
    static boost::property_tree::ptree ptEmpty;
    auto available_shells = pt.get_child(std::to_string(device->get_device_id()) + ".platform.available_shells", ptEmpty);

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

  DSAInfo dsa(image_path);

  // If the shell is not up-to-date and dsa has a flash image, queue the board for update
  boost::property_tree::ptree& pt_dev = pt.get_child(std::to_string(device->get_device_id()));
  bool same_shell = (dsa.name == pt_dev.get<std::string>("platform.current_shell.vbnv", ""))
                      && (dsa.matchId(pt_dev.get<std::string>("platform.current_shell.id", "")));

  auto sc = pt_dev.get<std::string>("platform.current_shell.sc_version", "");
  bool same_sc = ((sc.empty()) || (dsa.bmcVer.empty()) ||
                  (dsa.bmcVer == sc) || (sc.find("FIXED") != std::string::npos));

  // Always update Arista devices
  auto vendor = xrt_core::device_query<xrt_core::query::pcie_vendor>(device);
  if (vendor == ARISTA_ID)
    same_shell = false;

  if (XBU::getForce()) {
    same_shell = false;
    same_sc = false;
  }

  if (!same_shell || !same_sc) {
    if (!dsa.hasFlashImage)
      throw xrt_core::error("Flash image is not available");

    boardsToUpdate.push_back(std::make_pair(device->get_device_id(), dsa));
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
  report_status(dsa.name, pt_dev);

  // Continue to flash whatever we have collected in boardsToUpdate.
  bool needreboot = false;
  bool need_warm_reboot = false;
  std::stringstream report_stream;

  // Prompt user about what boards will be updated and ask for permission.
  if (!XBU::can_proceed(XBU::getForce()))
    return;

  // Perform DSA and BMC updating
  std::stringstream error_stream;
  for (auto& p : boardsToUpdate) {
    try {
      std::cout << std::endl;
      // 1) Flash the Satellite Controller image
      if (xrt_core::device_query<xrt_core::query::is_mfg>(device.get()) || xrt_core::device_query<xrt_core::query::is_recovery>(device.get()))
        report_stream << boost::format("  [%s] : Factory or Recovery image detected. Reflash the device after the reboot to update the SC firmware.\n") % getBDF(p.first);
      else {
        if (update_sc(p.first, p.second) == true)  {
          report_stream << boost::format("  [%s] : Successfully flashed the Satellite Controller (SC) image\n") % getBDF(p.first);
          need_warm_reboot = true;
        } else
          report_stream << boost::format("  [%s] : Satellite Controller (SC) is either up-to-date, fixed, or not installed. No actions taken.\n") % getBDF(p.first);
      }

      // 2) Flash shell image
      if (update_shell(p.first, p.second, flashType) == true)  {
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
    std::cout << "******************************************************************\n";
  }
}

void
OO_UpdateBase::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Update Base");

  XBUtilities::verbose("Option(s):");
  for (const auto & aString : _options)
    XBUtilities::verbose(std::string(" ") + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), false /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Only two images options are supported
  if (image.size() > 2)
    throw xrt_core::error("Multiple flash images provided. Please specify either 1 or 2 flash images.");

  // Populate flash type. Uses board's default when passing an empty input string.
  if (!flashType.empty()) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
        "Overriding flash mode is not recommended.\nYou may damage your device with this option.");
  }
  Flasher working_flasher(device->get_device_id());
  auto flash_type = working_flasher.getFlashType(flashType);

  XBU::verbose("Sub command: --base");
  XBU::sudo_or_throw("Root privileges are required to update the devices flash image");
  // User did not provide an image for all. Select image automatically.
  if (update.compare("all") == 0) {
    update_default_only(device.get(), false);
    if (image.empty()) {
      auto_flash(device, flash_type);
      return;
    }
  }
  else if (update.compare("no-backup") == 0) {
    if (image.empty()) {
      update_default_only(device.get(), true);
      auto_flash(device, flash_type);
      return;
    }
  }

  // All other cases have a specified image
  // Get a list of images known exist

  const auto validated_images = find_flash_image_paths(image);
  // Fail early here to reduce additional conditions below
  // Technically validated_images will never be empty as: if image is not empty but has a bad
  // path or bad shell name find_flash_image_paths exits early. This statement can be removed
  // or left here as a precaution.
  if (validated_images.empty())
    throw xrt_core::error("Please provide a valid xsabin file or specify the type of base to flash");

  std::map<std::string, std::string> validated_image_map;
  switch (validated_images.size()) {
    case 2:
      validated_image_map["primary"] = validated_images[0];
      validated_image_map["secondary"] = validated_images[1];
      break;
    case 1:
      validated_image_map["primary"] = validated_images[0];
      break;
    default:
      break;
  }

  if (update.compare("all") == 0) {
      update_default_only(device.get(), false);
      auto_flash(device, flash_type, validated_image_map["primary"]);
  }
  // For the following two if conditions regarding the validated images portion
  // The user may have provided an image, but, it may not exist or the shell name is wrong
  else if (update.compare("sc") == 0) {
    update_SC(device.get()->get_device_id(), validated_image_map["primary"]);
  }
  else if (update.compare("shell") == 0) {
    update_default_only(device.get(), false);
    update_shell(device.get()->get_device_id(), validated_image_map, flash_type);
    std::cout << "****************************************************\n";
    std::cout << "Cold reboot machine to load the new image on device.\n";
    std::cout << "****************************************************\n";
  }
  else if (update.compare("no-backup") == 0) {
    update_default_only(device.get(), true);
    auto_flash(device, flash_type, validated_image_map["primary"]);
  }
  else
    throw xrt_core::error("Usage: xbmgmt program --device='0000:00:00.0' --base [all|sc|shell]"
                          " --image=['/path/to/flash_image'|'shell name']");
}
