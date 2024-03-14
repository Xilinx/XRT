// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdExamineInternal.h"
#include "JSONConfigurable.h"
#include "core/common/error.h"

// Utilities
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenus.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>

static ReportCollection fullReportCollection = {};
static boost::program_options::options_description common_options;
static std::map<std::string,std::vector<std::shared_ptr<JSONConfigurable>>> jsonOptions;
static XBUtilities::VectorPairStrings common_reports;

// ----- C L A S S   M E T H O D S -------------------------------------------
SubCmdExamineInternal::SubCmdExamineInternal(bool _isHidden, bool _isDepricated, bool _isPreliminary, bool _isUserDomain, const boost::property_tree::ptree& configurations)
    : SubCmd("examine", 
             _isUserDomain ? "Status of the system and device" : "Returns detail information for the specified device.")
    , m_device("")
    , m_reportNames()
    , m_elementsFilter()
    , m_format("")
    , m_output("")
    , m_help(false)
    , m_isUserDomain(_isUserDomain)
{

  const std::string longDescription = "This command will 'examine' the state of the system/device and will"
                                      " generate a report of interest in a text or JSON format.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commandConfig = configurations;

  for (const auto& option : uniqueReportCollection)
    fullReportCollection.push_back(option);

  const auto& configs = JSONConfigurable::parse_configuration_tree(configurations);
  jsonOptions = JSONConfigurable::extract_subcmd_config<JSONConfigurable, Report>(fullReportCollection, configs, getConfigName(), std::string("report"));

  common_reports.emplace_back("all", "All known reports are produced");
  static const std::string reportOptionValues = XBU::create_suboption_list_map("", jsonOptions, common_reports);
  static const std::string formatOptionValues = XBU::create_suboption_list_string(Report::getSchemaDescriptionVector());
  common_options.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("format,f", boost::program_options::value<decltype(m_format)>(&m_format), (std::string("Report output format. Valid values are:\n") + formatOptionValues).c_str() )
    ("output,o", boost::program_options::value<decltype(m_output)>(&m_output), "Direct the output to the given file")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_commonOptions.add(common_options);
  m_commonOptions.add_options()
    ("report,r", boost::program_options::value<decltype(m_reportNames)>(&m_reportNames)->multitoken(), (std::string("The type of report to be produced. Reports currently available are:\n") + reportOptionValues).c_str() )
  ;
  
  if (m_isUserDomain)
    m_hiddenOptions.add_options()
      ("element,e", boost::program_options::value<decltype(m_elementsFilter)>(&m_elementsFilter)->multitoken(), "Filters individual elements(s) from the report. Format: '/<key>/<key>/...'")
    ;
}

void
SubCmdExamineInternal::print_help_internal() const
{
  if (m_device.empty())
    printHelp();
  else {
    const std::string deviceClass = XBU::get_device_class(m_device, m_isUserDomain);
    static const std::string reportOptionValues = XBU::create_suboption_list_map(deviceClass, jsonOptions, common_reports);
    std::vector<std::string> tempVec;
    common_options.add_options()
      ("report,r", boost::program_options::value<decltype(tempVec)>(&tempVec)->multitoken(), (std::string("The type of report to be produced. Reports currently available are:\n") + reportOptionValues).c_str() )
    ;
    printHelp(common_options, m_hiddenOptions, deviceClass);
  }
}

void
SubCmdExamineInternal::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: examine");

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  // Check to see if help was requested
  if (m_help) {
    print_help_internal();
    return;
  }
  
  // Determine report level
  std::vector<std::string> reportsToRun(m_reportNames);
  if (reportsToRun.empty()) {
    reportsToRun.push_back("host");
  }

  // DRC check
  // When json is specified, make sure an accompanying output file is also specified
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
    print_help_internal();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // DRC: Output file
  if (!m_output.empty() && std::filesystem::exists(m_output) && !XBU::getForce()) {
    std::cerr << boost::format("ERROR: The output file '%s' already exists.  Please either remove it or execute this command again with the '--force' option to overwrite it.") % m_output << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // -- Process the options --------------------------------------------
  ReportCollection reportsToProcess;            // Reports of interest

  // Filter out reports that are not compatible for the device
  const std::string deviceClass = XBU::get_device_class(m_device, m_isUserDomain);
  ReportCollection runnableReports = validateConfigurables<Report>(deviceClass, std::string("report"), fullReportCollection);

  // Collect the reports to be processed
  XBU::collect_and_validate_reports(runnableReports, reportsToRun, reportsToProcess);

  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  
  try {
    if(reportsToProcess.size() > 1 || reportsToRun.front().compare("host") != 0)
      device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), m_isUserDomain /*inUserDomain*/);
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

      std::cout << XBUtilities::str_available_devs(m_isUserDomain) << std::endl;
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
