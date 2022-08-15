/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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
#include "SubCmdExamineInternal.h"

// Utilities
#include "core/common/error.h"
#include "tools/common/Report.h"
#include "tools/common/XBHelpMenus.h"
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <fstream>
#include <iostream>


// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdExamineInternal::SubCmdExamineInternal(bool is_user_space)
    : SubCmd("examine", 
             "Returns detail information for the specified device."),
      m_is_user_space(is_user_space)
{
  const std::string long_description = "This command will 'examine' the state of the system/device and will"
                                      " generate a report of interest in a text or JSON format.";
  setLongDescription(long_description);
  setExampleSyntax("");
}

void
SubCmdExamineInternal::execute(const SubCmdOptions& _options) const
{
  // -- Build up the report & format options
  const std::string report_option_values = XBU::create_suboption_list_string(m_report_collection, true /*add 'all' option*/);
  const std::string format_option_values = XBU::create_suboption_list_string(Report::getSchemaDescriptionVector());

  // Option Variables
  std::string device_bdf;
  std::vector<std::string> report_names;
  std::vector<std::string> elements_filter;
  std::string sFormat = "";
  std::string sOutput = "";
  bool bHelp = false;

  // -- Retrieve and parse the subcommand options -----------------------------
  po::options_description common_options("Common Options");  
  common_options.add_options()
    ("device,d", boost::program_options::value<decltype(device_bdf)>(&device_bdf), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("report,r", boost::program_options::value<decltype(report_names)>(&report_names)->multitoken(), (std::string("The type of report to be produced. Reports currently available are:\n") + report_option_values).c_str() )
    ("format,f", boost::program_options::value<decltype(sFormat)>(&sFormat), (std::string("Report output format. Valid values are:\n") + format_option_values).c_str() )
    ("output,o", boost::program_options::value<decltype(sOutput)>(&sOutput), "Direct the output to the given file")
    ("help", boost::program_options::bool_switch(&bHelp), "Help to use this sub-command")
  ;

  po::options_description hidden_options("Hidden Options");

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options, common_options, hidden_options);

  // Check to see if help was requested 
  if (bHelp) {
    printHelp(common_options, hidden_options);
    return;
  }

  // Determine default reports
  if (report_names.empty()) {
    if (device_bdf.empty())
      report_names.push_back("host");
    else {
      report_names.push_back("platform");
      if (m_is_user_space)
        report_names.push_back("dynamic-regions");
    }
  }

  // When format specified, make sure an accompanying output file is also specified
  if (!sFormat.empty() && sOutput.empty()) {
    std::cerr << "ERROR: Please specify an output file to redirect the json to" << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  if (sFormat.empty())
    sFormat = "json";

  // DRC: Examine the output format
  Report::SchemaVersion schemaVersion = Report::getSchemaDescription(sFormat).schemaVersion;
  if (schemaVersion == Report::SchemaVersion::unknown) {
    std::cerr << boost::format("ERROR: Unsupported --format option value '%s'") % sFormat << std::endl
              << boost::format("       Supported values can be found in --format's help section below.") << std::endl;
    printHelp(common_options, hidden_options);
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // DRC: Output file
  if (!sOutput.empty() && boost::filesystem::exists(sOutput) && !XBU::getForce()) {
    std::cerr << boost::format("ERROR: The output file '%s' already exists.  Please either remove it or execute this command again with the '--force' option to overwrite it.") % sOutput << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // -- Process the options --------------------------------------------
  ReportCollection reportsToProcess;            // Reports of interest

  // Collect the reports to be processed
  XBU::collect_and_validate_reports(m_report_collection, report_names, reportsToProcess);

  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  
  try {
    if (reportsToProcess.size() > 1 || report_names.front().compare("host") != 0)
      device = XBU::get_device(boost::algorithm::to_lower_copy(device_bdf), m_is_user_space);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  bool is_report_output_valid = true;
  // DRC check on devices and reports
  if (!device) {
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
      const auto dev_pt = XBU::get_available_devices(m_is_user_space);
      if (dev_pt.empty())
        std::cout << "0 devices found" << std::endl;
      else
        std::cout << "Device list" << std::endl;

      for (const auto& kd : dev_pt) {
        const boost::property_tree::ptree& dev = kd.second;
        const std::string note = dev.get<bool>("is_ready") ? "" : "NOTE: Device not ready for use";
        std::cout << boost::format("  [%s] : %s %s\n") % dev.get<std::string>("bdf") % dev.get<std::string>("vbnv") % note;
      }
    }
  }

  // Create the report
  std::ostringstream oSchemaOutput;
  try {
    XBU::produce_reports(device, reportsToProcess, schemaVersion, elements_filter, std::cout, oSchemaOutput);
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
