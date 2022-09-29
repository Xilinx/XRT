// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

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

// -- Build up the report & format options
static const std::string reportOptionValues = XBU::create_suboption_list_string(fullReportCollection, true /*add 'all' option*/);
static const std::string formatOptionValues = XBU::create_suboption_list_string(Report::getSchemaDescriptionVector());

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdExamine::SubCmdExamine(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("examine", 
             "Returns detail information for the specified device.")
    , m_device("")
    , m_reportNames()
    , m_elementsFilter()
    , m_format("")
    , m_output("")
    , m_help(false)
{
  const std::string longDescription = "This command will 'examine' the state of the system/device and will"
                                      " generate a report of interest in a text or JSON format.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("report,r", boost::program_options::value<decltype(m_reportNames)>(&m_reportNames)->multitoken(), (std::string("The type of report to be produced. Reports currently available are:\n") + reportOptionValues).c_str() )
    ("format,f", boost::program_options::value<decltype(m_format)>(&m_format), (std::string("Report output format. Valid values are:\n") + formatOptionValues).c_str() )
    ("output,o", boost::program_options::value<decltype(m_output)>(&m_output), "Direct the output to the given file")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
SubCmdExamine::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: examine");

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  // Check to see if help was requested 
  if (m_help) {
    printHelp();
    return;
  }

  // Determine report leveld
  std::vector<std::string> reportsToRun(m_reportNames);
  if (reportsToRun.empty()) {
    if (m_device.empty())
      reportsToRun.push_back("host");
    else
      reportsToRun.push_back("platform");
  }

  // -- Process the options --------------------------------------------
  ReportCollection reportsToProcess;            // Reports of interest

  // Collect the reports to be processed
  XBU::collect_and_validate_reports(fullReportCollection, reportsToRun, reportsToProcess);

  // when json is specified, make sure an accompanying output file is also specified
  if (!m_format.empty() && m_output.empty())
    throw xrt_core::error("Please specify an output file to redirect the json to");

  const auto validated_format = m_format.empty() ? "json" : m_format;

  // Output Format
  Report::SchemaVersion schemaVersion = Report::getSchemaDescription(validated_format).schemaVersion;
  if (schemaVersion == Report::SchemaVersion::unknown) 
    throw xrt_core::error((boost::format("Unknown output format: '%s'") % validated_format).str());

  // Output file
  if (!m_output.empty() && boost::filesystem::exists(m_output) && !XBU::getForce()) 
      throw xrt_core::error((boost::format("Output file already exists: '%s'") % m_output).str());

  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  
  try {
    if(reportsToProcess.size() > 1 || reportsToRun.front().compare("host") != 0)
      device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), false /*inUserDomain*/);
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
    XBU::produce_reports(device, reportsToProcess, schemaVersion, m_elementsFilter, std::cout, oSchemaOutput);
  } catch (const std::exception&) {
    // Exception is thrown at the end of this function to allow for report writing
    is_report_output_valid = false;
  }

  // -- Write output file ----------------------------------------------
  if (!m_output.empty()) {
    std::ofstream fOutput;
    fOutput.open(m_output, std::ios::out | std::ios::binary);
    if (!fOutput.is_open()) 
      throw xrt_core::error((boost::format("Unable to open the file '%s' for writing.") % m_output).str());

    fOutput << oSchemaOutput.str();

    std::cout << boost::format("Successfully wrote the %s file: %s") % m_format % m_output << std::endl;
  }

  if (!is_report_output_valid)
    throw xrt_core::error(std::errc::operation_canceled);
}
