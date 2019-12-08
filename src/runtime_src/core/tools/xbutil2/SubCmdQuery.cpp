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
#include "SubCmdQuery.h"
#include "XBReport.h"
#include "XBDatabase.h"

#include "common/system.h"
#include "common/device.h"
#include "common/xclbin_parser.h"

#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ======= R E G I S T E R   T H E   S U B C O M M A N D ======================
#include "tools/common/SubCmd.h"
static const unsigned int registerResult =
                    register_subcommand("query",
                                        "Status of the system and device(s)",
                                        subCmdQuery);
// =============================================================================
#include "common/system.h"
#include "common/device.h"
#include <boost/format.hpp>

// ------ L O C A L   F U N C T I O N S ---------------------------------------


// ------ F U N C T I O N S ---------------------------------------------------
template <typename T>
std::vector<T> as_vector(boost::property_tree::ptree const& pt, 
                         boost::property_tree::ptree::key_type const& key)
{
    std::vector<T> r;

    boost::property_tree::ptree::const_assoc_iterator it = pt.find(key);

    if( it != pt.not_found()) {
      for (auto& item : pt.get_child(key)) {
        r.push_back(item.second);
      }
    }
    return r;
}

void pu1_query_report()
{
  boost::property_tree::ptree pt;
  xrt_core::get_devices(pt);

  XBU::trace_print_tree("Simple Device Tree", pt);

  // Now start to fill in the missing data

  std::vector<boost::property_tree::ptree> devices = as_vector<boost::property_tree::ptree>(pt, "devices");

  if (devices.size()) {
    std::cout << "----------------------------------------------------------------" << std::endl;
  }

  for (auto &ptDevice : devices) {
    int device_id = ptDevice.get<int>("device_id", -1);
    auto pDevice = xrt_core::get_userpf_device(device_id);

    std::cout << boost::format("%s : %d") % "Device ID" % device_id << std::endl;

    std::cout << "PCIe Interface" << std::endl;
    boost::property_tree::ptree ptPcie = ptDevice.get_child("pcie");

    std::cout << boost::format("  %-16s : %s") % "Vendor" % ptPcie.get<std::string>("vendor") << std::endl;
    std::cout << boost::format("  %-16s : %s") % "Device" % ptPcie.get<std::string>("device") << std::endl;
    std::cout << boost::format("  %-16s : %s") % "Subsystem Vendor" % ptPcie.get<std::string>("subsystem_vendor") << std::endl;
    std::cout << boost::format("  %-16s : %s") % "Subsystem ID" % ptPcie.get<std::string>("subsystem_id") << std::endl;
//    std::cout << boost::format("  %-16s : %s") % "Link Speed" % ptPcie.get<std::string>("link_speed") << std::endl;
//    std::cout << boost::format("  %-16s : %s (bits)") % "Data Width" % ptPcie.get<std::string>("width") << std::endl;

//    std::vector<boost::property_tree::ptree> threads = as_vector<boost::property_tree::ptree>(ptPcie, "dma_threads");
//    std::cout << boost::format("  %-16s : %d") % "DMA Thread Count" % threads.size() << std::endl;

    std::cout << std::endl;
    std::cout << "Feature ROM" << std::endl;
    boost::property_tree::ptree ptRom;
    pDevice->get_rom_info(ptRom);

    std::cout << boost::format("  %-16s : %s") % "VBNV" % ptRom.get<std::string>("vbnv") << std::endl;
    std::cout << boost::format("  %-16s : %s") % "FPGA" % ptRom.get<std::string>("fpga_name") << std::endl;

    std::cout << std::endl;
    std::cout << "Accelerator"  << std::endl;

  try {
    auto iplbuf = xrt_core::query_device<std::vector<char>>(pDevice, xrt_core::device::QR_IP_LAYOUT_RAW);
    auto iplayout = reinterpret_cast<const ip_layout*>(iplbuf.data());
    auto cus = xrt_core::xclbin::get_cus(iplayout);

    std::cout << "  Compute Unit(s)" << std::endl;

    if (cus.size() == 0) {
      std::cout << "    No compute units found." << std::endl;
    } else {
      int index = 0;
      for (auto cu : cus) {
        std::cout << boost::format("    [%d] - Base Address : 0x%x") % index++ % cu << std::endl;
      }
    }
  } catch (...) {
    std::cout << "   Accelerator metadata (e.g., xclbin) unavailable." <<  std::endl;
  }

  std::cout << "----------------------------------------------------------------" << std::endl;

  }
}


int subCmdQuery(const std::vector<std::string> &_options)
// Reference Command:  query [-d card [-r region]
{
  for (auto aString : _options) {
    std::cout << "Option: '" << aString << "'" << std::endl;
  }
  XBU::verbose("SubCommand: query");
  // -- Retrieve and parse the subcommand options -----------------------------
  xrt_core::device::id_type card = 0;
  bool help = false;

  po::options_description queryDesc("query options");
  queryDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    (",d", boost::program_options::value<decltype(card)>(&card), "Card to be examined.")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(queryDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cerr << queryDesc << std::endl;

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    std::cout << queryDesc << std::endl;
    return 0;
  }

  // -- Now process the subcommand --------------------------------------------

  // Report system configuration and XRT information
  XBReport::report_system_config();
  XBReport::report_xrt_info();

  pu1_query_report();

  // Gather the complete system information for ALL devices
  boost::property_tree::ptree pt;
  XBDatabase::create_complete_device_tree(pt);
  XBU::trace_print_tree("Complete Device Tree", pt);

  return registerResult;
}
