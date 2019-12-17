/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include "SubCmdFlash.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/common/device.h"
#include "core/common/system.h"
#include "core/common/error.h"
#include "flash/flasher.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ======= R E G I S T E R   T H E   S U B C O M M A N D ======================
#include "tools/common/SubCmd.h"
static const unsigned int registerResult =
                    register_subcommand("flash",
                                        "Update SC firmware or shell on the device",
                                        subCmdFlash);
// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

static unsigned int
bdf2index()
{
  return 0;//hardcoded for now
}
static void scan_devices(bool verbose, bool json)
{
  verbose = verbose;
  json = json;

  Flasher f(bdf2index());
  if (!f.isValid())
      return;

  DSAInfo board = f.getOnBoardDSA();
  std::vector<DSAInfo> installedDSA = f.getInstalledDSA();
  
  BoardInfo info;
  f.getBoardInfo(info);
  std::cout << "Card [" << f.sGetDBDF() << "]:\n";
  std::cout << "\tCard type:\t\t" << board.board << "\n";
  std::cout << "\tFlash type:\t\t" << f.sGetFlashType() << "\n";
  std::cout << "\tFlashable partition running on FPGA:" << "\n";
  std::cout << "\t\t" << board << "\n";
  std::cout << "\tFlashable partitions installed in system:\n";
  if (!installedDSA.empty())
	  std::cout << "\t\t" << installedDSA.front() << "\n";
  else
	  std::cout << "\t\tNone\n";

  if (verbose) {
	std::cout << "\tCard name\t\t\t" << info.mName << "\n";
	std::cout << "\tCard S/N: \t\t\t" << info.mSerialNum << "\n";
	std::cout << "\tConfig mode: \t\t" << info.mConfigMode << "\n";
	std::cout << "\tFan presence:\t\t" << info.mFanPresence << "\n";
	std::cout << "\tMax power level:\t\t" << info.mMaxPower << "\n";
	std::cout << "\tMAC address0:\t\t" << info.mMacAddr0 << "\n";
	std::cout << "\tMAC address1:\t\t" << info.mMacAddr1 << "\n";
	std::cout << "\tMAC address2:\t\t" << info.mMacAddr2 << "\n";
	std::cout << "\tMAC address3:\t\t" << info.mMacAddr3 << "\n";
  }

}

// Update shell on the board.
static void update_shell(unsigned index, std::string flashType,
    const std::string& primary, const std::string& secondary)
{
    std::shared_ptr<firmwareImage> pri;
    std::shared_ptr<firmwareImage> sec;

    if (!flashType.empty()) {
        std::cout << "CAUTION: Overriding flash mode is not recommended. " <<
            "You may damage your card with this option." << std::endl;
    }

    Flasher flasher(index);
    if(!flasher.isValid())
        return;

    if (primary.empty())
        return;

    pri = std::make_shared<firmwareImage>(primary.c_str(), MCS_FIRMWARE_PRIMARY);
    if (pri->fail())
        return;
    if (!secondary.empty()) {
        sec = std::make_shared<firmwareImage>(secondary.c_str(),
            MCS_FIRMWARE_SECONDARY);
        if (sec->fail())
            sec = nullptr;
    }

    flasher.upgradeFirmware(flashType, pri.get(), sec.get());
    std::cout << "Shell is updated succesfully\n";
    std::cout << "Cold reboot machine to load new shell on card" << std::endl;
}

static void update_SC(unsigned index, const std::string& file)
{
    Flasher flasher(index);
    if(!flasher.isValid())
        return;

    std::shared_ptr<firmwareImage> bmc =
        std::make_shared<firmwareImage>(file.c_str(), BMC_FIRMWARE);
    if (bmc->fail())
        return;

    flasher.upgradeBMCFirmware(bmc.get());
}

} // unnamed namespace


// ------ F U N C T I O N S ---------------------------------------------------

int subCmdFlash(const std::vector<std::string> &_options)
// Reference Command:   'flash' sub-command usage:
//                      --scan [--verbose|--json]
//                      --update [--shell name [--id id]] [--card bdf] [--force]
//                      --factory_reset [--card bdf]
//
//                      Experts only:
//                      --shell --path file --card bdf [--type flash_type]
//                      --sc_firmware --path file --card bdf


{
  XBU::verbose("SubCommand: flash");
  // -- Retrieve and parse the subcommand options -----------------------------
  bool help = false;
  bool scan = false;
  bool reset = false;
  bool update = false;
  bool shell = false;
  bool sc_firmware = false;


  po::options_description flashDesc("flash options");
  flashDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    ("scan", boost::program_options::bool_switch(&scan), "Information about the card")
    ("shell", boost::program_options::bool_switch(&shell), "Flash platform from source")
    ("sc_firmware", boost::program_options::bool_switch(&sc_firmware), "Flash sc firmware from source")
    //("factory_reset", boost::program_options::bool_switch(&reset), "Reset to golden image")
    //("update", boost::program_options::bool_switch(&update), "Update the card with the installed shell")
  ;

  // po::options_description expertsOnlyDesc("experts only");
  // expertsOnlyDesc.add_options()
  //   ("shell", boost::program_options::bool_switch(&shell), "Flash platform from source")
  //   ("sc_firmware", boost::program_options::bool_switch(&sc_firmware), "Flash sc firmware from source")
  // ;

  po::options_description allOptions("");
  allOptions.add(flashDesc);//.add(expertsOnlyDesc);

  // Parse the command line
  po::parsed_options parsed = po::command_line_parser(_options).
    options(allOptions).         // Global options
    allow_unregistered().           // Allow for unregistered options (needed for options)
    run();                          // Parse the options

  // Parse sub-command ...
  po::variables_map vm;
  try {
    po::store(parsed, vm); //Can throw
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    xrt_core::send_exception_message(e.what(), "XBMGMT");
    std::cerr << allOptions << std::endl;

    // Re-throw exception
    throw;
  }
  // Check to see if help was requested or no command was found
  if (help == true)  {
    std::cout << allOptions << std::endl;
    return 0;
  }

  //prep data
  std::vector<std::string> opts = po::collect_unrecognized(parsed.options, po::include_positional);

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(XBU::format("  Scan: %ld", scan));
  XBU::verbose(XBU::format("  Shell: %ld", shell));
  // XBU::verbose(XBU::format("  sc_firmware: %ld", sc_firmware));
  // XBU::verbose(XBU::format("  Reset: %ld", reset));
  // XBU::verbose(XBU::format("  Update: %ld", update));

  if (scan) {
    bool verbose;
    bool json = false;
    XBU::verbose("Sub command: --scan");

    po::options_description scanDesc("scan options");
    scanDesc.add_options()
      (",v", boost::program_options::bool_switch(&verbose), "verbose")
      //("json", boost::program_options::bool_switch(&json), "json")
    ;
    // -- Now process the subcommand options ----------------------------------
    po::variables_map option_vm;
    try {
      po::store(po::command_line_parser(opts).options(scanDesc).run(), option_vm);
      po::notify(option_vm); // Can throw
    } catch (po::error& e) {
      xrt_core::send_exception_message(e.what(), "XBMGMT");
      std::cerr << scanDesc << std::endl;
    // Re-throw exception
    throw;
    }

    // -- Now process the subcommand option-------------------------------
    XBU::verbose(XBU::format("  Verbose: %ld", verbose));
    //XBU::verbose(XBU::format("  Json: %ld", json));

    if (verbose && json) {
      XBU::error("Please specify only one option");
      return 1;
    }

    scan_devices(verbose, json);
    return registerResult;
  }

  if (update) {
    //--update [--shell name [--id id]] [--card bdf] [--force]
    bool force;
    std::string bdf;
    std::string name;
    std::string id;

    XBU::verbose("Sub command: --update");

    po::options_description updateDesc("update options");
    updateDesc.add_options()
      ("force", boost::program_options::bool_switch(&force), "force")
      ("card", boost::program_options::value<std::string>(&bdf), "bdf of the card")
      ("shell_name", boost::program_options::value<std::string>(&name), "name of shell")
      ("id", boost::program_options::value<std::string>(&id), "id of the card")
    ;

    // -- Now process the subcommand options ----------------------------------
    po::variables_map option_vm;
    try {
      po::store(po::command_line_parser(opts).options(updateDesc).run(), option_vm);
      po::notify(option_vm); // Can throw
    } catch (po::error& e) {
      xrt_core::send_exception_message(e.what(), "XBMGMT");
      std::cerr << updateDesc << std::endl;
    // Re-throw exception
    throw;
    }

    // -- Now process the subcommand option-------------------------------
    XBU::verbose(XBU::format("  Force: %ld", force));
    XBU::verbose(XBU::format("  Card: %s", bdf.c_str()));
    XBU::verbose(XBU::format("  Shell_name: %s", name.c_str()));
    XBU::verbose(XBU::format("  Card id: %s", id.c_str()));

    if (name.empty() && !id.empty()){
      XBU::error("Please specify the shell");
      return 1;
    }

    // auto device = xrt_core::get_mgmtpf_device(bdf2index());
    // device->auto_flash(name, id, force);
    return registerResult;
  }

  if (reset) {
    // --factory_reset [--card bdf]
    std::string bdf;

    XBU::verbose("Sub command: --factory_reset");

    po::options_description resetDesc("factory_reset options");
    resetDesc.add_options()
      ("card", boost::program_options::value<std::string>(&bdf), "bdf of the card");

    po::variables_map option_vm;
    try {
      po::store(po::command_line_parser(opts).options(resetDesc).run(), option_vm);
      po::notify(option_vm); // Can throw
    } catch (po::error& e) {
      xrt_core::send_exception_message(e.what(), "XBMGMT");
      std::cerr << resetDesc << std::endl;
    // Re-throw exception
    throw;
    }

    // -- Now process the subcommand option-------------------------------
    XBU::verbose(XBU::format("  Card: %s", bdf.c_str()));

    // auto device = xrt_core::get_mgmtpf_device(bdf2index());
    // device->reset_shell();
    return registerResult;
  }

  if (shell) {
    // --shell --path file --card bdf [--type flash_type]
    std::string bdf;
    std::string file;
    std::string flash_type;
    std::string secondary;

    XBU::verbose("Sub command: --shell");

    po::options_description shellDesc("shell options");
    shellDesc.add_options()
      ("path", boost::program_options::value<std::string>(&file), "path of shell file")
      // ("card", boost::program_options::value<std::string>(&bdf), "index of the card") //change this to bdf later
      // ("type", boost::program_options::value<std::string>(&flash_type), "flash_type")
    ;

    po::variables_map option_vm;
    try {
      po::store(po::command_line_parser(opts).options(shellDesc).run(), option_vm);
      po::notify(option_vm); // Can throw
    } catch (po::error& e) {
      xrt_core::send_exception_message(e.what(), "XBMGMT");
      std::cerr << shellDesc << "\n";
      std::cerr << "Example: xbmgmt.exe flash --shell --path='path\\to\\dsabin\\file'\n" << std::endl;
    // Re-throw exception
    throw;
    }

    // -- Now process the subcommand option-------------------------------
    // XBU::verbose(XBU::format("  Card: %s", bdf.c_str()));
    XBU::verbose(XBU::format("  File: %s", file.c_str()));
    // XBU::verbose(XBU::format("  Flash_type: %s", flash_type.c_str()));
    if (file.empty()) {// || bdf2index() == UINT_MAX) {
      XBU::error("Please specify the shell file path");// and the device bdf");
      std::cerr << shellDesc << "\n";
      std::cerr << "Example: xbmgmt.exe flash --shell --path='path\\to\\dsabin\\file'" << std::endl;
      return 1;
    }
    update_shell(bdf2index(), flash_type, file, secondary);
    return registerResult;
  }

  if (sc_firmware) {
    //--sc_firmware --path file --card bdf
    std::string bdf;
    std::string file;

    XBU::verbose("Sub command: --sc_firmware");

    po::options_description scDesc("sc_firmware options");
    scDesc.add_options()
      ("path", boost::program_options::value<std::string>(&file), "path of sc firmware file")
      //("card", boost::program_options::value<std::string>(&bdf), "bdf of the card")
    ;

    po::variables_map option_vm;
    try {
      po::store(po::command_line_parser(opts).options(scDesc).run(), option_vm);
      po::notify(option_vm); // Can throw
    } catch (po::error& e) {
      xrt_core::send_exception_message(e.what(), "XBMGMT");
      std::cerr << scDesc << std::endl;
    // Re-throw exception
    throw;
    }

    // -- Now process the subcommand option-------------------------------
    XBU::verbose(XBU::format("  Card: %s", bdf.c_str()));
    XBU::verbose(XBU::format("  Sc_file: %s", file.c_str()));
    if (file.empty()) {// || bdf2index() == UINT_MAX) {
      XBU::error("Please specify the sc file path");// and the device bdf");
      std::cerr << scDesc <<  "\n";
      std::cerr << "Example: xbmgmt.exe flash --sc_firmware --path='path\\to\\dsabin\\file'" << std::endl;
      return 1;
    }

    update_SC(bdf2index(), file);
    return registerResult;
  }

  return registerResult;
}
