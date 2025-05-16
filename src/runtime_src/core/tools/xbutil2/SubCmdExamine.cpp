// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdExamine.h"
#include "core/common/error.h"
#include "tools/common/XBHelpMenus.h"
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"

// ---- Reports ------
#include "tools/common/Report.h"
#include "tools/common/reports/ReportAie.h"
#include "tools/common/reports/ReportAieShim.h"
#include "tools/common/reports/ReportAieMem.h"
#include "tools/common/reports/ReportAiePartitions.h"
#include "tools/common/reports/ReportAsyncError.h"
#include "tools/common/reports/ReportBOStats.h"
#include "tools/common/reports/ReportClocks.h"
#include "tools/common/reports/ReportCmcStatus.h"
#include "tools/common/reports/ReportDynamicRegion.h"
#include "tools/common/reports/ReportDebugIpStatus.h"
#include "tools/common/reports/ReportElectrical.h"
#include "tools/common/reports/ReportFirewall.h"
#include "tools/common/reports/ReportHost.h"
#include "tools/common/reports/ReportMailbox.h"
#include "tools/common/reports/ReportMechanical.h"
#include "tools/common/reports/ReportMemory.h"
#include "tools/common/reports/ReportPcieInfo.h"
#include "tools/common/reports/ReportPreemption.h"
#include "tools/common/reports/platform/ReportPlatforms.h"
#include "tools/common/reports/ReportPsKernels.h"
#include "tools/common/reports/ReportQspiStatus.h"
#include "tools/common/reports/ReportTelemetry.h"
#include "tools/common/reports/ReportThermal.h"

#include <filesystem>
#include <fstream>

SubCmdExamine::SubCmdExamine(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("examine", "Status of the system and device")
{
  const std::string longDescription = "This command will 'examine' the state of the system/device and will"
                                      " generate a report of interest in a text or JSON format.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  // Note: Please insert the reports in the order to be displayed (alphabetical)
  uniqueReportCollection = {
  // Common reports
    std::make_shared<ReportAie>(),
    std::make_shared<ReportAieShim>(),
    std::make_shared<ReportAieMem>(),
    std::make_shared<ReportAiePartitions>(),
    std::make_shared<ReportAsyncError>(),
    std::make_shared<ReportBOStats>(),
    std::make_shared<ReportClocks>(),
    std::make_shared<ReportDebugIpStatus>(),
    std::make_shared<ReportDynamicRegion>(),
    std::make_shared<ReportHost>(),
    std::make_shared<ReportMemory>(),
    std::make_shared<ReportPcieInfo>(),
    std::make_shared<ReportPlatforms>(),
    std::make_shared<ReportPreemption>(),
    std::make_shared<ReportPsKernels>(),
  // Native only reports
  #ifdef ENABLE_NATIVE_SUBCMDS_AND_REPORTS
    std::make_shared<ReportElectrical>(),
    std::make_shared<ReportFirewall>(),
    std::make_shared<ReportMailbox>(),
    std::make_shared<ReportMechanical>(),
    std::make_shared<ReportQspiStatus>(),
    std::make_shared<ReportThermal>(),
    std::make_shared<ReportTelemetry>(),
  #endif
  };
}

void SubCmdExamine::fill_option_values(const po::variables_map& vm, SubCmdExamineOptions& options) const
{
  options.m_device = vm.count("device") ? vm["device"].as<std::string>() : "";
  options.m_format = vm.count("format") ? vm["format"].as<std::string>() : "JSON";
  options.m_output = vm.count("output") ? vm["output"].as<std::string>() : "";
  options.m_reportNames = vm.count("report") ? vm["report"].as<std::vector<std::string>>() : std::vector<std::string>();
  options.m_help = vm.count("help") ? vm["help"].as<bool>() : false;
  options.m_elementsFilter = vm.count("element") ? vm["element"].as<std::vector<std::string>>() : std::vector<std::string>(); 
}

void
SubCmdExamine::setOptionConfig(const boost::property_tree::ptree &config)
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
SubCmdExamine::getReportsList(const xrt_core::smi::tuple_vector& reports) const
{
  // Vector to store the matched reports
  std::vector<std::shared_ptr<Report>> matchedReports;

  for (const auto& rep : reports) {
    auto it = std::find_if(uniqueReportCollection.begin(), uniqueReportCollection.end(),
              [&rep](const std::shared_ptr<Report>& report) {
                return std::get<0>(rep) == report->getReportName() &&
                       (std::get<2>(rep) != "hidden" || XBU::getAdvance());
              });

    if (it != uniqueReportCollection.end()) {
      matchedReports.push_back(*it);
    }
  }

  return matchedReports;
}

void
SubCmdExamine::execute(const SubCmdOptions& _options) const
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
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Check to see if help was requested
  if (options.m_help) {
    printHelp();
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
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Determine report level
  std::vector<std::string> reportsToRun(options.m_reportNames);
  if (reportsToRun.empty()) {
    if (!XBU::getAdvance()) {
      reportsToRun.emplace_back("host");
    } 
    else {
      printHelp();
      return;
    }
  }

  if ((std::find(reportsToRun.begin(), reportsToRun.end(), "all") != reportsToRun.end()) && (reportsToRun.size() > 1)) {
    std::cerr << "ERROR: The 'all' value for the reports to run cannot be used with any other named reports.\n";
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // -- Process the options --------------------------------------------
  ReportCollection reportsToProcess;            // Reports of interest

 // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  
  try {
    if(reportsToRun.front().compare("host") != 0)
      device = XBU::get_device(boost::algorithm::to_lower_copy(options.m_device), true);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  ReportCollection runnableReports;
  if (device){
    const xrt_core::smi::tuple_vector& reportList = xrt_core::device_query<xrt_core::query::xrt_smi_lists>(device, xrt_core::query::xrt_smi_lists::type::examine_reports);
    runnableReports = getReportsList(reportList);
  } 
  else {
    runnableReports = uniqueReportCollection;
  }
  // Collect the reports to be processed
  try {
    XBU::collect_and_validate_reports(runnableReports, reportsToRun, reportsToProcess);
  } catch (const xrt_core::error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp();
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

      std::cout << XBUtilities::str_available_devs(true) << std::endl;
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