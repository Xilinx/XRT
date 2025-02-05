// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

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

// ----- C L A S S   M E T H O D S -------------------------------------------
SubCmdExamineInternal::SubCmdExamineInternal(bool _isHidden, bool _isDepricated, bool _isPreliminary, bool _isUserDomain, const boost::property_tree::ptree& configurations)
    : SubCmd("examine", 
             _isUserDomain ? "Status of the system and device" : "Returns detail information for the specified device.")
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
}

void
SubCmdExamineInternal::print_help_internal(const SubCmdExamineOptions& options) const
{
  if (options.m_device.empty())
    printHelp();
  else {
    const std::string deviceClass = XBU::get_device_class(options.m_device, m_isUserDomain);
    printHelp(m_commonOptions, m_hiddenOptions, deviceClass);
  }
}

void
SubCmdExamineInternal::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: examine");

  // Parse sub-command ...
  po::variables_map vm;
  SubCmdExamineOptions options;
  try{
    const auto unrecognized_options = process_arguments(vm, _options, false);
    fill_option_values(vm, options);

    if (!unrecognized_options.empty())
    {
      std::string error_str;
      error_str.append("Unrecognized arguments:\n");
      for (const auto& option : unrecognized_options)
        error_str.append(boost::str(boost::format("  %s\n") % option));
      throw boost::program_options::error(error_str);
    }
  }
  catch (const boost::program_options::error& e)
  {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    print_help_internal(options);
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Check to see if help was requested
  if (options.m_help) {
    print_help_internal(options);
    return;
  }

  Report::SchemaVersion schemaVersion = Report::getSchemaDescription(options.m_format).schemaVersion;
  try{
    if (vm.count("output") && options.m_output.empty())
      throw xrt_core::error("Output file not specified");

    if (vm.count("report") && options.m_reportNames.empty())
      throw xrt_core::error("No report given to be produced");

    if (vm.count("element") && options.m_elementsFilter.empty())
      throw xrt_core::error("No element filter given to be produced");

    if (schemaVersion == Report::SchemaVersion::unknown) 
      throw xrt_core::error((boost::format("Unknown output format: '%s'") % options.m_format).str());

    // DRC check
    // When json is specified, make sure an accompanying output file is also specified
    if (vm.count("format") && options.m_output.empty())
      throw xrt_core::error("Please specify an output file to redirect the json to");

    if (!options.m_output.empty() && std::filesystem::exists(options.m_output) && !XBU::getForce())
      throw xrt_core::error((boost::format("The output file '%s' already exists. Please either remove it or execute this command again with the '--force' option to overwrite it") % options.m_output).str());

  } catch (const xrt_core::error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    print_help_internal(options);
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Determine report level
  std::vector<std::string> reportsToRun(options.m_reportNames);
  if (reportsToRun.empty()) {
    reportsToRun.push_back("host");
  }

  // -- Process the options --------------------------------------------
  ReportCollection reportsToProcess;            // Reports of interest

  // Filter out reports that are not compatible for the device
  const std::string deviceClass = XBU::get_device_class(options.m_device, m_isUserDomain);

 // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  
  try {
    if(reportsToProcess.size() > 1 || reportsToRun.front().compare("host") != 0)
      device = XBU::get_device(boost::algorithm::to_lower_copy(options.m_device), m_isUserDomain /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  ReportCollection runnableReports;
  if (device){
    const xrt_core::smi::tuple_vector& reportList = xrt_core::device_query<xrt_core::query::xrt_smi_lists>(device, xrt_core::query::xrt_smi_lists::type::examine_reports);
    runnableReports = getReportsList(reportList);
  } else 
  {
    runnableReports = validateConfigurables<Report>(deviceClass, std::string("report"), fullReportCollection);
  }
  // Collect the reports to be processed
  try {
    XBU::collect_and_validate_reports(runnableReports, reportsToRun, reportsToProcess);
  } catch (const xrt_core::error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    print_help_internal(options);
    return;
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
    XBU::produce_reports(device, reportsToProcess, schemaVersion, options.m_elementsFilter, std::cout, oSchemaOutput);
  } catch (const std::exception&) {
    // Exception is thrown at the end of this function to allow for report writing
    is_report_output_valid = false;
  }

  // -- Write output file ----------------------------------------------
  if (!options.m_output.empty()) {
    std::ofstream fOutput;
    fOutput.open(options.m_output, std::ios::out | std::ios::binary);
    if (!fOutput.is_open()) {
      std::cerr << boost::format("Unable to open the file '%s' for writing.") % options.m_output << std::endl;
      throw xrt_core::error(std::errc::operation_canceled);
    }

    fOutput << oSchemaOutput.str();

    std::cout << boost::format("Successfully wrote the %s file: %s") % options.m_format % options.m_output << std::endl;
  }

  if (!is_report_output_valid)
    throw xrt_core::error(std::errc::operation_canceled);
}
void SubCmdExamineInternal::fill_option_values(const po::variables_map& vm, SubCmdExamineOptions& options) const
{
  options.m_device = vm.count("device") ? vm["device"].as<std::string>() : "";
  options.m_format = vm.count("format") ? vm["format"].as<std::string>() : "JSON";
  options.m_output = vm.count("output") ? vm["output"].as<std::string>() : "";
  options.m_reportNames = vm.count("report") ? vm["report"].as<std::vector<std::string>>() : std::vector<std::string>();
  options.m_help = vm.count("help") ? vm["help"].as<bool>() : false;
  options.m_elementsFilter = vm.count("element") ? vm["element"].as<std::vector<std::string>>() : std::vector<std::string>(); 
}

void
SubCmdExamineInternal::setOptionConfig(const boost::property_tree::ptree &config)
{
  m_jsonConfig = SubCmdJsonObjects::JsonConfig(config.get_child("subcommands"), getName());
  try{
    m_jsonConfig.addProgramOptions(m_commonOptions, "common", getName());
    m_jsonConfig.addProgramOptions(m_hiddenOptions, "hidden", getName());
  } 
  catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}

std::vector<std::shared_ptr<Report>>
SubCmdExamineInternal::getReportsList(const xrt_core::smi::tuple_vector& reports) const
{
  // Vector to store the matched reports
  std::vector<std::shared_ptr<Report>> matchedReports;

  for (const auto& rep : reports) {
    auto it = std::find_if(fullReportCollection.begin(), fullReportCollection.end(),
              [&rep](const std::shared_ptr<Report>& report) {
                return std::get<0>(rep) == report->getReportName() &&
                       (std::get<2>(rep) != "hidden" || XBU::getShowHidden());
              });

    if (it != fullReportCollection.end()) {
      matchedReports.push_back(*it);
    }
  }

  return matchedReports;
}
