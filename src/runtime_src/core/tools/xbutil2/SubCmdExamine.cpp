// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

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
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>
#include <regex>

// ---- Reports ------
#include "tools/common/Report.h"
#include "tools/common/ReportAie.h"
#include "tools/common/ReportAieShim.h"
#include "tools/common/ReportAieMem.h"
#include "tools/common/ReportAsyncError.h"
#include "tools/common/ReportBOStats.h"
#include "tools/common/ReportCmcStatus.h"
#include "tools/common/ReportDynamicRegion.h"
#include "tools/common/ReportDebugIpStatus.h"
#include "tools/common/ReportElectrical.h"
#include "tools/common/ReportFirewall.h"
#include "tools/common/ReportHost.h"
#include "tools/common/ReportMailbox.h"
#include "tools/common/ReportMechanical.h"
#include "tools/common/ReportMemory.h"
#include "tools/common/ReportPcieInfo.h"
#include "tools/common/ReportPlatforms.h"
#include "tools/common/ReportPsKernels.h"
#include "tools/common/ReportQspiStatus.h"
#include "tools/common/ReportThermal.h"

// Note: Please insert the reports in the order to be displayed (alphabetical)
  static ReportCollection fullReportCollection = {
  // Common reports
    std::make_shared<ReportAie>(),
    std::make_shared<ReportAieShim>(),
    std::make_shared<ReportAieMem>(),
    std::make_shared<ReportAsyncError>(),
    std::make_shared<ReportBOStats>(),
    std::make_shared<ReportDebugIpStatus>(),
    std::make_shared<ReportDynamicRegion>(),
    std::make_shared<ReportHost>(),
    std::make_shared<ReportMemory>(),
    std::make_shared<ReportPcieInfo>(),
    std::make_shared<ReportPlatforms>(),
    std::make_shared<ReportPsKernels>(),
  // Native only reports
  #ifdef ENABLE_NATIVE_SUBCMDS_AND_REPORTS
    std::make_shared<ReportElectrical>(),
    std::make_shared<ReportFirewall>(),
    std::make_shared<ReportMailbox>(),
    std::make_shared<ReportMechanical>(),
    std::make_shared<ReportQspiStatus>(),
    std::make_shared<ReportThermal>(),
  #endif
  };

// ----- C L A S S   M E T H O D S -------------------------------------------
static const std::string reportOptionValues = XBU::create_suboption_list_string(fullReportCollection, true /*add 'all' option*/);
static const std::string formatOptionValues = XBU::create_suboption_list_string(Report::getSchemaDescriptionVector());

SubCmdExamine::SubCmdExamine(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("examine",
             "Status of the system and device")
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
  setIsDefaultDevValid(false);

  m_commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.\n")
    ("report,r", boost::program_options::value<decltype(m_reportNames)>(&m_reportNames)->multitoken(), (std::string("The type of report to be produced. Reports currently available are:\n") + reportOptionValues).c_str() )
    ("format,f", boost::program_options::value<decltype(m_format)>(&m_format), (std::string("Report output format. Valid values are:\n") + formatOptionValues).c_str() )
    ("output,o", boost::program_options::value<decltype(m_output)>(&m_output), "Direct the output to the given file")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_hiddenOptions.add_options()
    ("element,e", boost::program_options::value<decltype(m_elementsFilter)>(&m_elementsFilter)->multitoken(), "Filters individual elements(s) from the report. Format: '/<key>/<key>/...'")
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

  // -- Determine default values --
  
  // Report default value
  std::vector<std::string> reportsToRun(m_reportNames);
  if (m_reportNames.empty()) {
    if (m_device.empty())
      reportsToRun.push_back("host");
    else {
      reportsToRun.push_back("platform");
      reportsToRun.push_back("dynamic-regions");
    }
  }

  // DRC check
  // When  is specified, make sure an accompanying output file is also specified
  if (!m_format.empty() && m_output.empty()) {
    std::cerr << "ERROR: Please specify an output file to redirect the json to" << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  const auto validated_format = m_format.empty() ? "json" : m_format;

  // DRC: Examine the output format
  Report::SchemaVersion schemaVersion = Report::getSchemaDescription(validated_format).schemaVersion;
  if (schemaVersion == Report::SchemaVersion::unknown) {
    std::cerr << boost::format("ERROR: Unsupported --format option value '%s'") % validated_format << std::endl
              << boost::format("       Supported values can be found in --format's help section below.") << std::endl;
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // DRC: Output file
  if (!m_output.empty() && boost::filesystem::exists(m_output) && !XBU::getForce()) {
    std::cerr << boost::format("ERROR: The output file '%s' already exists.  Please either remove it or execute this command again with the '--force' option to overwrite it.") % m_output << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

// -- Process the options --------------------------------------------
  ReportCollection reportsToProcess;            // Reports of interest

  bool is_report_output_valid = true;
  // Collect the reports to be processed
  XBU::collect_and_validate_reports(fullReportCollection, reportsToRun, reportsToProcess);

  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  
  try {
    if(reportsToProcess.size() > 1 || reportsToRun.front().compare("host") != 0)
      device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

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

      for(auto& kd : dev_pt) {
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
    if (!fOutput.is_open()) {
      std::cerr << boost::format("Unable to open the file '%s' for writing.") % m_output << std::endl;
      throw xrt_core::error(std::errc::operation_canceled);
    }

    fOutput << oSchemaOutput.str();

    std::cout << boost::format("Successfully wrote the %s file: %s") % validated_format % m_output << std::endl;
  }

  if (!is_report_output_valid)
    throw xrt_core::error(std::errc::operation_canceled);
}
