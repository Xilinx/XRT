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
#include "SubCmdDmaTest.h"
#include "tools/common/ReportValidate.h"
#include "tools/common/ReportHost.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenus.h"
#include "core/common/query_requests.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/format.hpp>
#include <boost/any.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

/*
 * helper function for kernelVersionTest
 */
static void
check_os_release(const std::vector<std::string> kernel_versions, const std::string& release, boost::property_tree::ptree& _ptDevice)
{
  for (const auto& ver : kernel_versions) {
        if (release.find(ver) != std::string::npos) {
          _ptDevice.put("status", "passed");
          return;
        }
    }
    _ptDevice.put("status", "passed with warning");
    _ptDevice.put("warning_msg", boost::str(boost::format("Kernel verison %s is not officially supported. %s is the latest supported version") 
      % release % kernel_versions.back()));
}

/*
 * TEST #1
 */
void
kernelVersionTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptDevice) 
{
  _ptDevice.put("name", "Kernel version");
  _ptDevice.put("description", "Check if kernel version is supported by XRT");

  //please append the new supported versions
  const std::vector<std::string> ubuntu_kernel_versions = { "4.4.0", "4.13.0", "4.15.0", "4.18.0", "5.0.0", "5.3.0" };
  const std::vector<std::string> centos_rh_kernel_versions = { "3.10.0-693", "3.10.0-862", "3.10.0-957", "3.10.0-1062" };

  boost::property_tree::ptree _pt_host;
  std::make_shared<ReportHost>()->getPropertyTreeInternal(_dev.get(), _pt_host);
  const std::string os = _pt_host.get<std::string>("host.os.distribution");
  const std::string release = _pt_host.get<std::string>("host.os.release");

  if(os.find("Ubuntu") != std::string::npos) {
    check_os_release(ubuntu_kernel_versions, release, _ptDevice);
  }
  else if(os.find("Red Hat") != std::string::npos || os.find("CentOS") != std::string::npos) {
    check_os_release(centos_rh_kernel_versions, release, _ptDevice);
  }
  else {
    _ptDevice.put("status", "failed");
  }
}

/*
 * TEST #2
 */
void
auxConnectionTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptDevice) 
{
  _ptDevice.put("name", "Aux connection");
  _ptDevice.put("description", "Check if auxiliary power is connected");

  const std::vector<std::string> auxPwrRequiredDevice = { "VCU1525", "U200", "U250", "U280" };
  
  uint16_t max_power = 0;
  auto name = xrt_core::device_query<xrt_core::query::xmc_board_name>(_dev);
  max_power = xrt_core::device_query<xrt_core::query::xmc_max_power>(_dev);

  //check if device has aux power connector
  bool auxDevice = false;
  for (auto bd : auxPwrRequiredDevice) {
    if (name.find(bd) != std::string::npos) {
      auxDevice = true;
      break;
    }
  }

  if (!auxDevice) {
      _ptDevice.put("info", "Aux power connector is not available on this board");
      _ptDevice.put("status", "skipped");
      return;
  }

  //check aux cable if board u200, u250, u280
  if(max_power == 0) {
    _ptDevice.put("warning_msg", "Aux power is not connected");
    _ptDevice.put("info", "Device is not stable for heavy acceleration tasks");
    _ptDevice.put("status", "passed with warning");
  } 
  else {
    _ptDevice.put("status", "passed");
  }
}

/*
 * TEST #3
 */
void
pcieLinkTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptDevice) 
{
  _ptDevice.put("name", "PCIE link");
  _ptDevice.put("description", "Check if PCIE link is active");
  auto speed     = xrt_core::device_query<xrt_core::query::pcie_link_speed>(_dev);
  auto max_speed = xrt_core::device_query<xrt_core::query::pcie_link_speed_max>(_dev);
  auto width     = xrt_core::device_query<xrt_core::query::pcie_express_lane_width>(_dev);
  auto max_width = xrt_core::device_query<xrt_core::query::pcie_express_lane_width_max>(_dev);
  if (speed != max_speed || width != max_width) {
    _ptDevice.put("warning_msg", "Link is active");
    _ptDevice.put("info", boost::format("Please make sure that the device is plugged into Gen %dx%d, instead of Gen %dx%d. %s.")
                                          % max_speed % max_width % speed % width % "Lower performance maybe experienced");
    _ptDevice.put("status", "passed with warning");
  }
  else {
    _ptDevice.put("status", "passed");
  }
}

/*
 * TEST #4
 */
void
scVersionTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptDevice) 
{
  _ptDevice.put("name", "SC version");
  _ptDevice.put("description", "Check if SC firmware is up-to-date");

  auto sc_ver     = xrt_core::device_query<xrt_core::query::xmc_bmc_version>(_dev);
  std::string exp_sc_ver = "";
  try{
    exp_sc_ver = xrt_core::device_query<xrt_core::query::expected_bmc_version>(_dev);
  } catch(...) {}

  if (!exp_sc_ver.empty() && sc_ver.compare(exp_sc_ver) != 0) {
    _ptDevice.put("warning_msg", "SC firmware misatch");
    _ptDevice.put("info", boost::format("SC firmware version %s is running on the board, but SC firmware version %s is expected from the installed shell. %s.") 
                                          % sc_ver % exp_sc_ver % "Please use xbmgmt --new status to check the installed shell");
    _ptDevice.put("status", "passed with warning");
  }
  else {
    _ptDevice.put("status", "passed");
  }
}

/*
 * TEST #5
 */
void
verifyKernelTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptDevice) 
{
  _ptDevice.put("name", "Verify kernel");
  _ptDevice.put("description", "Run 'Hello World' kernel test");
  _ptDevice.put("status", "skipped");
}

/*
 * TEST #6
 */
void
dmaTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptDevice) 
{
  _ptDevice.put("name", "DMA");
  _ptDevice.put("description", "Run dma test ");
  _ptDevice.put("status", "skipped");
  // std::make_shared<SubCmdDmaTest>(true,true,true)->execute(std::vector<std::string>{"-d", std::to_string(_dev.get()->get_device_id())});
}

/*
 * TEST #7
 */
void
bandwidthKernelTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptDevice) 
{
  _ptDevice.put("name", "Bandwidth kernel");
  _ptDevice.put("description", "Check if auxiliary power is connected");
  _ptDevice.put("status", "skipped");
}

/*
 * TEST #8
 */
void
p2pTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptDevice) 
{
  _ptDevice.put("name", "Peer to peer bar");
  _ptDevice.put("description", "Check if auxiliary power is connected");
  _ptDevice.put("status", "skipped");
}

/*
 * TEST #9
 */
void
m2mTest(const std::shared_ptr<xrt_core::device>& _dev, boost::property_tree::ptree& _ptDevice) 
{
  _ptDevice.put("name", "Memory to memory DMA");
  _ptDevice.put("description", "Check if auxiliary power is connected");
  _ptDevice.put("status", "skipped");
}

// list of tests
using TestCollection = std::vector<std::function<void(const std::shared_ptr<xrt_core::device>&, boost::property_tree::ptree&)>>;
static const TestCollection testSuite = {
  kernelVersionTest,
  auxConnectionTest,
  pcieLinkTest,
  scVersionTest,
  verifyKernelTest,
  dmaTest,
  bandwidthKernelTest,
  p2pTest,
  m2mTest
};

// list of ptrees for each test outputs
boost::property_tree::ptree _ptKernelVersion;
boost::property_tree::ptree _ptAuxConnection;
boost::property_tree::ptree _ptpcieLinkTest;
boost::property_tree::ptree _ptscVersionTest;
boost::property_tree::ptree _ptverifyKernelTest;
boost::property_tree::ptree _ptdmaTest;
boost::property_tree::ptree _ptbandwidthKernelTest;
boost::property_tree::ptree _ptp2pTest;
boost::property_tree::ptree _ptm2mTest;
static std::vector<boost::property_tree::ptree> ptInfo = { 
  _ptKernelVersion, 
  _ptAuxConnection,
  _ptpcieLinkTest,
  _ptscVersionTest,
  _ptverifyKernelTest,
  _ptdmaTest,
  _ptbandwidthKernelTest,
  _ptp2pTest,
  _ptm2mTest,
};


/*
 * helper function for pretty_print_tests
 */
static void print_status(int passed, int total, bool warning)
{
  std::cout << (passed*100)/total << "% tests passed";
  if(warning)
    std::cout << ", but with warnings";
  std::cout << std::endl << std::endl;;
}

/*
 * helper function: print one validated device in user friendly format
 */
static void
pretty_print_dev_validate(const boost::property_tree::ptree& _ptDeviceInfo) 
{
  std::cout << boost::format("Validate device[%s]") % _ptDeviceInfo.get<std::string>("device_id") << std::endl;
  boost::property_tree::ptree tests = _ptDeviceInfo.get_child("tests");
  int passed = 0;
  bool warning = false;
  for(auto& kv : tests) {
    boost::property_tree::ptree& test = kv.second;
    std::cout << boost::format("%d/%d Test #%-10d: %s\n") % test.get<int>("id") % testSuite.size() 
                    % test.get<int>("id") % test.get<std::string>("name");
    std::cout << boost::format("    %-16s: %s\n") % "Description" % test.get<std::string>("description");
    try{
      std::cout << boost::format("    %-16s: %s\n") % "Warning" % test.get<std::string>("warning_msg");
    }
    catch(...) {}
    try{
      std::cout << boost::format("    %-16s: %s\n") % "Error" % test.get<std::string>("error_msg");
    }
    catch(...) {}
    try{
      std::string formatted_output;
      XBU::wrap_paragraph(test.get<std::string>("info"), 22, 70, false, formatted_output);
      std::cout << boost::format("    %-16s: %s\n") % "Details" % formatted_output;
    }
    catch(...) {}
    std::string status = test.get<std::string>("status");
    if(status.find("warning") != std::string::npos)
      warning = true;
    boost::to_upper(status);
    std::cout << boost::format("    [%s]\n") % status;
    std::cout << "------------------------------------------------------------------" << std::endl;

    //a test is considered success if it passes or is skipped
    if(test.get<std::string>("status").find("passed") != std::string::npos || 
        test.get<std::string>("status").compare("skipped") == 0)  
      passed++;
  }
  print_status(passed, testSuite.size(), warning);
}

/*
 * print all validated devices in user friendly format
 */
static void 
pretty_print_tests(const boost::property_tree::ptree& _ptInfo)
{
  boost::property_tree::ptree _ptDevices = _ptInfo.get_child("logical_devices");
  std::cout << boost::format("Starting validation for %d devices\n") % _ptDevices.size() << std::endl;
  for(auto& kv : _ptDevices) {
    boost::property_tree::ptree& _ptDeviceInfo = kv.second;
    pretty_print_dev_validate(_ptDeviceInfo);
  }
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
  std::vector<std::string> device  = {"all"};
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

  boost::property_tree::ptree _ptValidate;
  boost::property_tree::ptree _ptDevCollectionTestSuite;
  for(auto const& device : deviceCollection) {
    boost::property_tree::ptree _ptDeviceTestSuite;
    boost::property_tree::ptree _ptDeviceInfo;
    auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);
    _ptDeviceInfo.put("device_id", xrt_core::query::pcie_bdf::to_string(bdf));
    
    for(unsigned int i = 0; i < testSuite.size(); i++) {
      ptInfo[i].put("id", i+1);
      testSuite[i](device, ptInfo[i]);
      _ptDeviceTestSuite.push_back( std::make_pair("", ptInfo[i]) );
      //if a test fails, exit immideately 
      if(ptInfo[i].get<std::string>("status").compare("failed") == 0)
        break;
    }
    _ptDeviceInfo.put_child("tests", _ptDeviceTestSuite);
    _ptDevCollectionTestSuite.push_back( std::make_pair("", _ptDeviceInfo) );
  }
  _ptValidate.put_child("logical_devices", _ptDevCollectionTestSuite);
  
  

  
  //DEBUG:
  std::stringstream ss;
  boost::property_tree::json_parser::write_json(ss, _ptValidate);
  std::cout << ss.str() << std::endl;

  pretty_print_tests(_ptValidate); //pass in ostr
}

