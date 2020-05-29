/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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
#include "SubCmdValidate.h"
#include "tools/common/ReportValidate.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenus.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

// static bool
// check_os_release(const std::vector<std::string> kernel_versions, xrt_core::device _dev, boost::property_tree::ptree& _ptDevice)
// {
//     const std::string release = sensor_tree::get<std::string>("system.release");
//     for (const auto& ver : kernel_versions) {
//         if (release.find(ver) != std::string::npos)
//             return true;
//     }
//     ostr << "WARNING: Kernel verison " << release << " is not officially supported. " 
//         << kernel_versions.back() << " is the latest supported version" << std::endl;
//     return false;
// }

static bool
is_supported_kernel_version(xrt_core::device* _dev, boost::property_tree::ptree& _ptDevice)
{
    // std::vector<std::string> ubuntu_kernel_versions =
    //     { "4.4.0", "4.13.0", "4.15.0", "4.18.0", "5.0.0", "5.3.0" };
    // std::vector<std::string> centos_rh_kernel_versions =
    //     { "3.10.0-693", "3.10.0-862", "3.10.0-957", "3.10.0-1062" };
    // const std::string os = sensor_tree::get<std::string>("system.linux", "N/A");

    // if(os.find("Ubuntu") != std::string::npos)
    //     return check_os_release(ubuntu_kernel_versions, _dev, _ptDevice);
    // else if(os.find("Red Hat") != std::string::npos || os.find("CentOS") != std::string::npos)
    //     return check_os_release(centos_rh_kernel_versions, _dev, _ptDevice);
    
    return true;
}

void
kernelVersionTest(xrt_core::device* _dev, boost::property_tree::ptree& _ptDevice) 
{
  _ptDevice.put("name", "Kernel version");
  _ptDevice.put("description", "Check if kernel version is supported by XRT");
    if (!is_supported_kernel_version(_dev, _ptDevice)) {
        _ptDevice.put("status", "failed");
    }
    _ptDevice.put("status", "passed");
}

}
//end anonymous namespace

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdValidate::SubCmdValidate(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("validate", 
             "Validates the basic shell accelleration functionality")
{
  const std::string longDescription = "Validates the given card by executing the platform's validate executable.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}


void
SubCmdValidate::execute(const SubCmdOptions& _options) const

{
  XBU::verbose("SubCommand: validate");
  // -- Retrieve and parse the subcommand options -----------------------------
  std::vector<std::string> device;
  bool help = false;

  po::options_description commonOptions("Commmon Options");
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(device)>(&device)->multitoken(), "The device of interest. This is specified as follows:\n"
                                                                           "  <BDF> - Bus:Device.Function (e.g., 0000:d8:00.0)\n"
                                                                           "  all   - Examines all known devices (default)")

    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  po::options_description hiddenOptions("Hidden Options");

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

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  // -- process "device" option -----------------------------------------------
  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  for (const auto & deviceName : device) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection);

  boost::property_tree::ptree _ptDevice;

  kernelVersionTest(deviceCollection.front().get(), _ptDevice);

  // std::stringstream ss;
  // boost::property_tree::json_parser::write_json(ss, _ptDevice);
  // std::cout << ss.str() << std::endl;


  // XBU::produce_reports(deviceCollection, {std::make_shared<ReportValidate>()}, Report::SchemaVersion::json_20201, elementsFilter, std::cout);
}

