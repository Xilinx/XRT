/**
 * Copyright (C) 2020 Xilinx, Inc
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
#include "SubCmdStatus.h"
#include "core/common/error.h"

// Utilities
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
#include "ReportPlatform.h"

// Note: Please insert the reports in the order to be displayed (current alphabetical)
static const ReportCollection fullReportCollection = {
  std::make_shared<ReportPlatform>()
};

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdStatus::SubCmdStatus(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("status", 
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
SubCmdStatus::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: status");

  // -- Build up the report & format options
  const std::string reportOptionValues = XBU::create_suboption_list_string(fullReportCollection, true /*add 'verbose' option*/);
  const std::string formatOptionValues = XBU::create_suboption_list_string(Report::getSchemaDescriptionVector());

  // Option Variables
  std::vector<std::string> devices = {"all"};
  std::vector<std::string> reportNames = {"platform"};
  std::vector<std::string> elementsFilter;
  std::string sFormat = "text";
  std::string sOutput = "";
  bool bHelp = false;

  // -- Retrieve and parse the subcommand options -----------------------------
  po::options_description queryDesc("Options");  // Note: Boost will add the colon.
  queryDesc.add_options()
    ("device,d", boost::program_options::value<decltype(devices)>(&devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.  A value of 'all' (default) indicates that every found device should be examined.")
    ("report,r", boost::program_options::value<decltype(reportNames)>(&reportNames)->multitoken(), (std::string("The type of report to be produced. Reports currently available are:\n") + reportOptionValues).c_str() )
    ("format,f", boost::program_options::value<decltype(sFormat)>(&sFormat), (std::string("Report output format. Valid values are:\n") + formatOptionValues).c_str() )
    ("element,e", boost::program_options::value<decltype(elementsFilter)>(&elementsFilter)->multitoken(), "Filters individual elements(s) from the report.")
    ("output,o", boost::program_options::value<decltype(sOutput)>(&sOutput), "Direct the output to the given file")
    ("help,h", boost::program_options::bool_switch(&bHelp), "Help to use this sub-command")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(queryDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(queryDesc);
    throw; // Re-throw exception
  }

  // Check to see if help was requested 
  if (bHelp == true)  {
    printHelp(queryDesc);
    return;
  }

  // -- Process the options --------------------------------------------
  ReportCollection reportsToProcess;            // Reports of interest
  xrt_core::device_collection deviceCollection;  // The collection of devices to examine
  Report::SchemaVersion schemaVersion = Report::SchemaVersion::unknown;    // Output schema version

  try {
    // Collect the reports to be processed
    XBU::collect_and_validate_reports(fullReportCollection, reportNames, reportsToProcess);

    // Output Format
    schemaVersion = Report::getSchemaDescription(sFormat).schemaVersion;
    if (schemaVersion == Report::SchemaVersion::unknown) 
      throw xrt_core::error((boost::format("Unknown output format: '%s'") % sFormat).str());

    // Output file
    if (!sOutput.empty() && boost::filesystem::exists(sOutput)) 
        throw xrt_core::error((boost::format("Output file already exists: '%s'") % sOutput).str());

    // Collect all of the devices of interest
    std::set<std::string> deviceNames;
    for (const auto & deviceName : devices) 
      deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

    XBU::collect_devices(deviceNames, false /*inUserDomain*/, deviceCollection);

    // DRC check on devices and reports
    if (deviceCollection.empty()) {
      std::vector<std::string> missingReports;
      for (const auto & report : reportsToProcess) {
        if (report->isDeviceRequired())
          missingReports.push_back(report->getReportName());
      }
      if (!missingReports.empty()) {
        std::cout << boost::format("Warning: Due to missing devices, the following reports will not be generated:\n");
        for (const auto & report : missingReports) 
          std::cout << boost::format("         - %s\n") % report;
      }
    }
  } catch (const xrt_core::error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp(queryDesc);
    return;
  }

  // -- Create the reports ------------------------------------------------
  if (sOutput.empty()) {
    XBU::produce_reports(deviceCollection, reportsToProcess, schemaVersion, elementsFilter, std::cout);
  }
  else {
    std::ofstream fOutput;
    fOutput.open(sOutput, std::ios::out | std::ios::binary);
    if (!fOutput.is_open()) 
      throw xrt_core::error((boost::format("Unable to open the file '%s' for writing.") % sOutput).str());

    XBU::produce_reports(deviceCollection, reportsToProcess, schemaVersion, elementsFilter, fOutput);

    fOutput.close();
  }
}
