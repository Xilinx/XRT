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
#include "SubCmdExamine.h"
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
#include "tools/common/ReportCu.h"
#include "tools/common/ReportFirewall.h"
#include "tools/common/ReportDebugIpStatus.h"
#include "tools/common/ReportElectrical.h"
#include "tools/common/ReportMechanical.h"
#include "tools/common/ReportAie.h"
#include "tools/common/ReportMemory.h"
#include "tools/common/ReportThermal.h"
#include "tools/common/ReportAsyncError.h"
#include "tools/common/ReportPlatforms.h"
#include "tools/common/ReportPcieInfo.h"
#include "tools/common/ReportMailbox.h"
#include "tools/common/ReportQspiStatus.h"

// Note: Please insert the reports in the order to be displayed (alphabetical)
  static ReportCollection fullReportCollection = {
  // Common reports
    std::make_shared<ReportAie>(),
    std::make_shared<ReportMemory>(),
    std::make_shared<ReportHost>(),
    std::make_shared<ReportCu>(),
    std::make_shared<ReportDebugIpStatus>(),
    std::make_shared<ReportAsyncError>(),
    std::make_shared<ReportPcieInfo>(),
    std::make_shared<ReportPlatforms>(),
  // Native only reports
  #ifdef ENABLE_NATIVE_SUBCMDS_AND_REPORTS
    std::make_shared<ReportElectrical>(),
    std::make_shared<ReportMailbox>(),
    std::make_shared<ReportMechanical>(),
    std::make_shared<ReportFirewall>(),
    std::make_shared<ReportThermal>(),
    std::make_shared<ReportQspiStatus>(),
  #endif
  };

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdExamine::SubCmdExamine(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("examine", 
             "Status of the system and device(s)")
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
  std::vector<std::string> devices = {"all"};
  std::vector<std::string> reportNames = {"host"};
  std::vector<std::string> elementsFilter;
  std::string sFormat = "text";
  std::string sOutput = "";
  bool bHelp = false;

  // -- Retrieve and parse the subcommand options -----------------------------
  po::options_description commonOptions("Common Options");  
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(devices)>(&devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.  A value of 'all' (default) indicates that every found device should be examined.")
    ("report,r", boost::program_options::value<decltype(reportNames)>(&reportNames)->multitoken(), (std::string("The type of report to be produced. Reports currently available are:\n") + reportOptionValues).c_str() )
    ("format,f", boost::program_options::value<decltype(sFormat)>(&sFormat), (std::string("Report output format. Valid values are:\n") + formatOptionValues).c_str() )
    ("output,o", boost::program_options::value<decltype(sOutput)>(&sOutput), "Direct the output to the given file")
    ("help,h", boost::program_options::bool_switch(&bHelp), "Help to use this sub-command")
  ;

  po::options_description hiddenOptions("Hidden Options");  
  hiddenOptions.add_options()
    ("element,e", boost::program_options::value<decltype(elementsFilter)>(&elementsFilter)->multitoken(), "Filters individual elements(s) from the report. Format: '/<key>/<key>/...'")
  ;

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
    return;
  }

  // Check to see if help was requested 
  if (bHelp == true)  {
    printHelp(commonOptions, hiddenOptions);
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

    XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection);

    // DRC check on devices and reports
    if (deviceCollection.empty()) {
      std::vector<std::string> missingReports;
      for (const auto & report : reportsToProcess) {
        if (report->isDeviceRequired())
          missingReports.push_back(report->getReportName());
      }
      if (!missingReports.empty()) {

        auto dev_pt = XBU::get_available_devices(true);
        if(dev_pt.empty())
          std::cout << "0 devices found" << std::endl;
        else 
          std::cout << "Device list" << std::endl;

        for(auto& kd : dev_pt) {
          boost::property_tree::ptree& dev = kd.second;
          std::string note = dev.get<bool>("is_ready") ? "" : "NOTE: Device not ready for use";
          std::cout << boost::format("  [%s] : %s %s\n") % dev.get<std::string>("bdf") % dev.get<std::string>("vbnv") % note;
        }
        
        std::cout << boost::format("Warning: Due to missing devices, the following reports will not be generated:\n");
        for (const auto & report : missingReports) 
          std::cout << boost::format("         - %s\n") % report;
      }
    }
  } catch (const xrt_core::error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp(commonOptions, hiddenOptions);
    return;
  }
  catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
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
