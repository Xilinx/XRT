/**
 * Copyright (C) 2022 Licensed under the Apache License, Version
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
#include "OO_BOStats.h"
#include "core/common/query_requests.h"
#include "core/common/system.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/range/as_array.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_BOStats::OO_BOStats( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Show usage stats of all BO types" )
    , m_device({})
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  //m_positionalOptions. No extra options
}

void
OO_BOStats::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand option: Show usage stats of all BO types");

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
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  //-- Working variables
  std::shared_ptr<xrt_core::device> device;

  try {
    // enforce 1 device 
    if (m_device.empty() || m_device.size() > 1) {
      std::cerr << "\nERROR: Please specify a single device using --device option\n\n";
      std::cout << "List of available devices:" << std::endl;
      boost::property_tree::ptree available_devices = XBU::get_available_devices(true);
      for(auto& kd : available_devices) {
        boost::property_tree::ptree& _dev = kd.second;
        std::cout << boost::format("  [%s] : %s\n") % _dev.get<std::string>("bdf") % _dev.get<std::string>("vbnv");
      }
      std::cout << std::endl;
      throw xrt_core::error(std::errc::operation_canceled);
    }
    
    // Collect the device of interest
    std::set<std::string> deviceNames;
    xrt_core::device_collection deviceCollection;
    for (const auto & deviceName : m_device) 
      deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));
    
    XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection); // Can throw
    // set working variable
    device = deviceCollection.front();

  } catch (const xrt_core::error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  catch (const std::runtime_error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  XBU::verbose(boost::str(boost::format("Device: %s") % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device))));

  try{
    auto mem_stat_char = xrt_core::device_query<xrt_core::query::memstat>(device);
    std::vector<std::string> mem_stat;
    boost::split(mem_stat, mem_stat_char, boost::is_any_of(boost::as_array("\n\0")));
    bool found = false;

    for (auto& line: mem_stat) {
      if (line.empty())
        continue;

      if (found) {
        std::vector<std::string> bo_info;
        boost::split(bo_info, line, boost::is_any_of("\t "));
        if (bo_info.size() != 3)
          throw xrt_core::error((boost::format("ERROR: Unexpected format in BO Stats. Line: %s") % line).str());

        XBU::verbose(boost::str(boost::format("BO type: %-11s, Total size(KB): %-8s, Num of BOs: %-5s") 
          % bo_info[0].substr(1, bo_info[0].length()-2)
          % bo_info[1].substr(0, bo_info[1].length()-2)
          % bo_info[2].substr(0, bo_info[2].length()-3)));
        continue;
      }
      if (line.find("BO Stats Below") != std::string::npos)
        found = true;
    }
    if (!found) 
      throw xrt_core::error("ERROR: BO Stats not found");
    
  } catch(const xrt_core::error& e) {
    std::cerr << e.what() << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }
  XBU::verbose("Show BO stats succeeded");
}

