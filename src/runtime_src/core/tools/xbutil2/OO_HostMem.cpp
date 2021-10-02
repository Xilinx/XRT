/**
 * Copyright (C) 2021 Licensed under the Apache License, Version
 * 2.0 (the "License"). You may not use this file except in
 * compliance with the License. A copy of the License is located
 * at
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
#include "OO_HostMem.h"
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
    , m_devices({})
    , m_action("")
    , m_size("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_devices)>(&m_devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("action", boost::program_options::value<decltype(m_action)>(&m_action)->required(), "Action to perform: ENABLE or DISABLE")
    ("size,s", boost::program_options::value<decltype(m_size)>(&m_size), "Size of host memory (bytes) to be enabled (e.g. 256M, 1G)")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
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
    XBUtilities::verbose(std::string(" ") + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(m_optionsDescription).positional(m_positionalOptions).run(), vm);
    po::notify(vm); // Can throw
  }
  catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  if(m_help) {
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  // Exit if neither action or device specified
  if(m_action.empty()) {
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  if(m_devices.empty()) {
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  uint64_t size = 0;
  try {
    if(!m_size.empty())
      size = XBUtilities::string_to_bytes(m_size);
  } 
  catch(const xrt_core::error&) {
    std::cerr << "Value supplied to --size option is invalid. Please specify a memory size between 4M and 1G." << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  bool enable = false;
  try {
    if ((boost::iequals(m_action, "ENABLE") != 0) && (boost::iequals(m_action, "DISABLE") != 0)) {
      std::cerr << boost::format("ERROR: Invalid action value: '%s'\n") % m_action;
      printHelp();
      throw xrt_core::error(std::errc::operation_canceled);
    }
    enable =  boost::iequals(m_action, "ENABLE");

    // Exit if ENABLE action is specified but size is not
    if(enable && size == 0)
      throw xrt_core::error(std::errc::invalid_argument, "Please specify a non-zero memory size between 4M and 1G as a power of 2.");

    // Collect all of the devices of interest
    std::set<std::string> deviceNames;
    xrt_core::device_collection deviceCollection;
    for (const auto & deviceName : m_devices) 
      deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));
    
    XBUtilities::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection);

    // enforce 1 device specification
    if(deviceCollection.size() > 1) {
      std::stringstream errmsg;
      errmsg << "Multiple devices are not supported. Please specify a single device using --device option\n\n";
      errmsg << "List of available devices:\n";
      boost::property_tree::ptree available_devices = XBUtilities::get_available_devices(true);
      for(auto& kd : available_devices) {
        boost::property_tree::ptree& _dev = kd.second;
        errmsg << boost::format("  [%s] : %s\n") % _dev.get<std::string>("bdf") % _dev.get<std::string>("vbnv");
      }
      throw xrt_core::error(std::errc::operation_canceled, errmsg.str());
    }

    //Set host-mem
    host_mem(deviceCollection[0].get(), enable, size);
  } 
  catch(const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  catch (const std::runtime_error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  std::cout << boost::format("\nHost-mem %s successfully\n") % (enable ? "enabled" : "disabled");
}
