// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_HostMem.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

namespace {

void
host_mem(xrt_core::device* device, bool action, uint64_t size)
{
  XBUtilities::sudo_or_throw("Root privileges required to enable host-mem");
  device->set_cma(action, size); //can throw
}

} //end namespace 

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_HostMem::OO_HostMem( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Controls host-mem functionality")
    , m_device("")
    , m_action("")
    , m_size("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("action", boost::program_options::value<decltype(m_action)>(&m_action)->required(), "Action to perform: ENABLE or DISABLE")
    ("size,s", boost::program_options::value<decltype(m_size)>(&m_size), "Size of host memory (bytes) to be enabled (e.g. 256M, 1G)")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("action", 1 /* max_count */)
  ;
}

void
OO_HostMem::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Host Mem");

  XBUtilities::verbose("Option(s):");
  for (auto & aString : _options)
    XBUtilities::verbose(" " + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  if(m_help) {
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  // Exit if neither action or device specified
  if(m_action.empty()) {
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  if(m_device.empty()) {
    std::cerr << boost::format("ERROR: A device needs to be specified.\n");
    throw xrt_core::error(std::errc::operation_canceled);
  }

  uint64_t size = 0;
  try {
    if(!m_size.empty())
      size = XBUtilities::string_to_base_units(m_size, XBUtilities::unit::bytes);
  } 
  catch(const xrt_core::error&) {
    std::cerr << "Value supplied to --size option is invalid. Please specify a memory size between 4M and 1G." << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  try {
    
    if (!boost::iequals(m_action, "ENABLE") && !boost::iequals(m_action, "DISABLE")) {
      std::cerr << boost::format("ERROR: Invalid action value: '%s'\n") % m_action;
      printHelp();
      throw xrt_core::error(std::errc::operation_canceled);
    }
    bool enable = boost::iequals(m_action, "ENABLE");

    // Exit if ENABLE action is specified and 
    // size is not 0 or size is not a power of 2
    if(enable && ((size == 0) || !XBUtilities::is_power_of_2(size)))
      throw xrt_core::error(std::errc::invalid_argument, "Please specify a non-zero memory size between 4M and 1G as a power of 2.");
    
    // Find device of interest
    auto device = XBUtilities::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
    //Set host-mem
    host_mem(device.get(), enable, size);
    std::cout << boost::format("\nHost-mem %s successfully\n") % (enable ? "enabled" : "disabled");
  }
  catch(const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  catch (const std::runtime_error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}
