// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdReset.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/common/query_requests.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ----- C L A S S   M E T H O D S -------------------------------------------

static void
pretty_print_action_list(xrt_core::device* device, xrt_core::query::reset_type& reset)
{
  std::cout << "Performing '" << reset.get_name() << "' on " << std::endl;
  std::cout << boost::format("  -[%s]\n") % 
               xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));

  if (!reset.get_warning().empty())
    std::cout << "WARNING: " << reset.get_warning() << std::endl;
}

static void 
reset_ecc(const std::shared_ptr<xrt_core::device>& dev, xrt_core::query::reset_type& reset)
{
  auto raw_mem = xrt_core::device_query<xrt_core::query::mem_topology_raw>(dev);
  const mem_topology *map = (mem_topology *)raw_mem.data();
    if(raw_mem.empty() || map->m_count == 0) {
      std::cout << "WARNING: 'mem_topology' not found, "
        << "unable to query ECC info. Has the xclbin been loaded? "
        << "See 'xbmgmt status'." << std::endl;
      return;
    }

    for(int32_t i = 0; i < map->m_count; i++) {
      if(!map->m_mem_data[i].m_used)
        continue;
      reset.set_subdev(reinterpret_cast<const char *>(map->m_mem_data[i].m_tag));
      dev->reset(reset);
    }
}

static void
reset_device(const std::shared_ptr<xrt_core::device>& dev, xrt_core::query::reset_type& reset)
{  
  if(reset.get_key() == xrt_core::query::reset_key::ecc)
    reset_ecc(dev, reset);
  else
    dev->reset(reset);
  std::cout << boost::format("Successfully reset Device[%s]\n")
    % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(dev));
}

static void
supported(std::string resetType) {
  std::vector<std::string> vec { "hot", "kernel", "ert", "ecc", "soft-kernel", "aie" };
  std::vector<std::string>::iterator it;
  it = std::find (vec.begin(), vec.end(), resetType);
  if (it == vec.end()) {
    throw xrt_core::error(-ENODEV, "reset not supported");
  }
}

SubCmdReset::SubCmdReset(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("reset", 
             "Resets the given device")
{
  const std::string longDescription = "Resets the given device.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_hiddenOptions.add_options()
    ("type,t", boost::program_options::value<decltype(m_resetType)>(&m_resetType)->notifier(supported), "The type of reset to perform. Types resets available:\n"
                                                                        "  kernel       - Kernel communication links\n" 
                                                                        "  ert          - Reset management processor\n"
                                                                        "  ecc          - Reset ecc memory\n"
                                                                        "  soft-kernel  - Reset soft kernel");

}

void
SubCmdReset::execute(const SubCmdOptions& _options) const
// Reference Command:  reset [-d card]

{
  XBU::verbose("SubCommand: reset");

  po::options_description commonOptions("Common Options");


  po::options_description hiddenOptions("Hidden Options");

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  // Check to see if help was requested or no command was found
  if (m_help) {
    printHelp();
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(boost::str(boost::format("  Reset: %s") % m_resetType));

  // -- process "device" option -----------------------------------------------
  // Find device of interest
  std::shared_ptr<xrt_core::device> device;


  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), false /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  
  xrt_core::query::reset_type type = XBU::str_to_reset_obj(m_resetType);
  pretty_print_action_list(device.get(), type);

  // Ask user for permission
  XBUtilities::sudo_or_throw("Root privileges are required to perform management resets");
  if(!XBU::can_proceed(XBU::getForce()))
    throw xrt_core::error(std::errc::operation_canceled);

  //perform reset action
  reset_device(device, type);
}
