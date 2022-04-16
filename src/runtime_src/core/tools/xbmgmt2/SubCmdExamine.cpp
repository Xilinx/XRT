/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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
#include "SubCmdExamine.h"
#include "core/common/error.h"

// Utilities
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenus.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// ---- Reports ------
#include "tools/common/Report.h"
#include "tools/common/ReportHost.h"
#include "tools/common/ReportFirewall.h"
#include "tools/common/ReportMechanical.h"
#include "tools/common/ReportMailbox.h"
#include "tools/common/ReportCmcStatus.h"
#include "tools/common/ReportVmrStatus.h"
#include "ReportPlatform.h"

// Note: Please insert the reports in the order to be displayed (current alphabetical)
static const ReportCollection fullReportCollection = {
  // Common reports
    std::make_shared<ReportHost>(false),
    std::make_shared<ReportPlatform>(),
  // Native only reports
  #ifdef ENABLE_NATIVE_SUBCMDS_AND_REPORTS
    std::make_shared<ReportMechanical>(),
    std::make_shared<ReportFirewall>(),
    std::make_shared<ReportMailbox>(),
    std::make_shared<ReportCmcStatus>(),
    std::make_shared<ReportVmrStatus>()
  #endif
};

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdExamine::SubCmdExamine(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("examine", 
             "Returns detail information for the specified device.")
{
  const std::string longDescription = "This command will 'examine' the state of the system/device and will"
                                      " generate a report of interest in a text or JSON format.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdExamine::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: examine");

  // -- Build up the report & format options
  const std::string reportOptionValues = XBU::create_suboption_list_string(fullReportCollection, true /*add 'all' option*/);
  const std::string formatOptionValues = XBU::create_suboption_list_string(Report::getSchemaDescriptionVector());

  // Option Variables
  std::vector<std::string> devices;
  std::vector<std::string> reportNames;
  std::vector<std::string> elementsFilter;
  std::string sFormat = "";
  std::string sOutput = "";
  bool bHelp = false;

  // -- Retrieve and parse the subcommand options -----------------------------
  po::options_description commonOptions("Common Options");  
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(devices)>(&devices), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("report,r", boost::program_options::value<decltype(reportNames)>(&reportNames)->multitoken(), (std::string("The type of report to be produced. Reports currently available are:\n") + reportOptionValues).c_str() )
    ("format,f", boost::program_options::value<decltype(sFormat)>(&sFormat), (std::string("Report output format. Valid values are:\n") + formatOptionValues).c_str() )
    ("output,o", boost::program_options::value<decltype(sOutput)>(&sOutput), "Direct the output to the given file")
    ("help", boost::program_options::bool_switch(&bHelp), "Help to use this sub-command")
  ;


  po::options_description allOptions("All Options");  
  allOptions.add(commonOptions);

  // Parse sub-command ...
  po::variables_map vm;

  process_arguments(vm, _options, commonOptions);

  // Check to see if help was requested 
  if (bHelp == true)  {
    printHelp(commonOptions);
    return;
  }

  // Determine report leveld
  if (reportNames.empty()) {
    if (devices.empty())
      reportNames.push_back("host");
    else
      reportNames.push_back("platform");
  }

  // -- Process the options --------------------------------------------
  ReportCollection reportsToProcess;            // Reports of interest
  xrt_core::device_collection deviceCollection;  // The collection of devices to examine
  Report::SchemaVersion schemaVersion = Report::SchemaVersion::unknown;    // Output schema version

  // Collect the reports to be processed
  XBU::collect_and_validate_reports(fullReportCollection, reportNames, reportsToProcess);

  // when json is specified, make sure an accompanying output file is also specified
  if (!sFormat.empty() && sOutput.empty())
    throw xrt_core::error("Please specify an output file to redirect the json to");

  if(sFormat.empty())
    sFormat = "json";

  // Output Format
  schemaVersion = Report::getSchemaDescription(sFormat).schemaVersion;
  if (schemaVersion == Report::SchemaVersion::unknown) 
    throw xrt_core::error((boost::format("Unknown output format: '%s'") % sFormat).str());

  // Output file
  if (!sOutput.empty() && boost::filesystem::exists(sOutput) && !XBU::getForce()) 
      throw xrt_core::error((boost::format("Output file already exists: '%s'") % sOutput).str());

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  for (const auto & deviceName : devices) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  XBU::collect_devices(deviceNames, false /*inUserDomain*/, deviceCollection);

  // enforce 1 device specification if multiple reports are requested
  if(deviceCollection.size() > 1 && (reportsToProcess.size() > 1 || reportNames.front().compare("host") != 0)) {
    std::cerr << "\nERROR: Examining multiple devices is not supported. Please specify a single device using --device option\n\n";
    std::cout << "List of available devices:" << std::endl;
    const boost::property_tree::ptree available_devices = XBU::get_available_devices(false);
    for(const auto& kd : available_devices) {
      const boost::property_tree::ptree& _dev = kd.second;
      std::cout << boost::format("  [%s] : %s\n") % _dev.get<std::string>("bdf") % _dev.get<std::string>("vbnv");
    }
    std::cout << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  bool is_report_output_valid = true;
  // DRC check on devices and reports
  if (deviceCollection.empty()) {
    std::vector<std::string> missingReports;
    for (const auto & report : reportsToProcess) {
      if (report->isDeviceRequired())
        missingReports.push_back(report->getReportName());
    }

    if (!missingReports.empty()) {
      // Exception is thrown at the end of this function to allow for report writing
      is_report_output_valid = false;
      // Print error message
      std::cerr << boost::format("Error: The following report(s) require specifying a device using the --device option:\n");
      for (const auto & report : missingReports)
        std::cout << boost::format("         - %s\n") % report;

      // Print available devices
      const auto dev_pt = XBU::get_available_devices(true);
      if(dev_pt.empty())
        std::cout << "0 devices found" << std::endl;
      else
        std::cout << "Device list" << std::endl;

      for(const auto& kd : dev_pt) {
        const boost::property_tree::ptree& dev = kd.second;
        const std::string note = dev.get<bool>("is_ready") ? "" : "NOTE: Device not ready for use";
        std::cout << boost::format("  [%s] : %s %s\n") % dev.get<std::string>("bdf") % dev.get<std::string>("vbnv") % note;
      }
    }
  }

  // Create the report
  std::ostringstream oSchemaOutput;
  try {
    XBU::produce_reports(deviceCollection, reportsToProcess, schemaVersion, elementsFilter, std::cout, oSchemaOutput);
  } catch (const std::exception&) {
    // Exception is thrown at the end of this function to allow for report writing
    is_report_output_valid = false;
  }

  // -- Write output file ----------------------------------------------
  if (!sOutput.empty()) {
    std::ofstream fOutput;
    fOutput.open(sOutput, std::ios::out | std::ios::binary);
    if (!fOutput.is_open()) 
      throw xrt_core::error((boost::format("Unable to open the file '%s' for writing.") % sOutput).str());

    fOutput << oSchemaOutput.str();

    std::cout << boost::format("Successfully wrote the %s file: %s") % sFormat % sOutput << std::endl;
  }

  if (!is_report_output_valid)
    throw xrt_core::error(std::errc::operation_canceled);
}
