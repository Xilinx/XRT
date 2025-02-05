// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "core/common/time.h"
#include "core/common/query_requests.h"
#include "XBHelpMenusCore.h"
#include "XBUtilitiesCore.h"
#include "XBHelpMenus.h"
#include "XBUtilities.h"
namespace XBU = XBUtilities;


// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <algorithm>
#include <iostream>
#include <numeric>

// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBUtilities;

// ------ S T A T I C   V A R I A B L E S -------------------------------------
static unsigned int m_maxColumnWidth = 100;
static unsigned int m_shortDescriptionColumn = 24;

// ------ F U N C T I O N S ---------------------------------------------------
static std::string 
create_suboption_list_map_string(const std::map<std::string, VectorPairStrings>& _collection)
{
  // Working variables
  const unsigned int maxColumnWidth = m_maxColumnWidth - m_shortDescriptionColumn; 
  std::string supportedValues;        // Formatted string of supported values
                                      
  // Make a copy of the data (since it is going to be modified)
  auto workingCollection = _collection;

  // Determine the indention width
  unsigned int maxStringLength = 0;
  for (auto& map_pair : workingCollection) {
    for (auto& pairs : map_pair.second) {
      // Determine if the keyName needs to have 'quotes', if so add them
      if (pairs.first.find(' ') != std::string::npos ) {
        pairs.first.insert(0, 1, '\'');  
        pairs.first += "\'";     
      }

      maxStringLength = std::max<unsigned int>(maxStringLength, static_cast<unsigned int>(pairs.first.length()));
    }
  }

  const unsigned int indention = maxStringLength + 5;  // New line indention after the '-' character (5 extra spaces)
  boost::format reportFmt(std::string("  %-") + std::to_string(maxStringLength) + "s - %s");  
  boost::format reportFmtQuotes(std::string(" %-") + std::to_string(maxStringLength + 1) + "s - %s");

  // Print out common options first
  const auto common_options = workingCollection.find("common");
  // Report names and description
  if (_collection.size() >= 2)
    supportedValues += "Common:\n";

  for (const auto& pairs : common_options->second) {
    boost::format &reportFormat = pairs.first[0] == '\'' ? reportFmtQuotes : reportFmt;
    auto formattedString = XBU::wrap_paragraphs(boost::str(reportFormat % pairs.first % pairs.second), indention, maxColumnWidth, false /*indent first line*/);
    supportedValues += formattedString + "\n";
  }
  workingCollection.erase(common_options);

  // Print out all other device specific options
  for (const auto& [device_class, test_list] : workingCollection) {

    // Report names and description
    std::stringstream help_data;
    for (const auto& pairs : test_list) {
      boost::format &reportFormat = pairs.first[0] == '\'' ? reportFmtQuotes : reportFmt;
      auto formattedString = XBU::wrap_paragraphs(boost::str(reportFormat % pairs.first % pairs.second), indention, maxColumnWidth, false /*indent first line*/);
      help_data << formattedString + "\n";
    }

    const std::string help_data_str = help_data.str();
    if (!help_data_str.empty()) {
      std::string device_string = device_class;
      if (device_string.empty())
        XBU::throw_cancel("Missing device class string");

      device_string[0] = static_cast<char>(std::toupper(device_string[0]));\

      supportedValues += device_string + ":\n";

      supportedValues += help_data_str;
    }
  }

  return supportedValues;
}

std::string 
XBUtilities::create_suboption_list_string(const VectorPairStrings &_collection)
{
  std::map<std::string, VectorPairStrings> collection = {
    {"common", _collection}
  };
  return create_suboption_list_map_string(collection);
}

std::string 
XBUtilities::create_suboption_list_string( const ReportCollection &_reportCollection, 
                                           bool _addAllOption)
{
  VectorPairStrings reportDescriptionCollection;

  // Add the report names and description
  for (const auto & report : _reportCollection) {
    // Skip hidden reports
    if (!XBU::getShowHidden() && report->isHidden()) 
      continue;
    reportDescriptionCollection.emplace_back(report->getReportName(), report->getShortDescription());
  }

  // 'verbose' option
  if (_addAllOption) 
    reportDescriptionCollection.emplace_back("all", "All known reports are produced");

  // Sort the collection
  sort(reportDescriptionCollection.begin(), reportDescriptionCollection.end(), 
       [](const std::pair<std::string, std::string> & a, const std::pair<std::string, std::string> & b) -> bool
       { return (a.first.compare(b.first) < 0); });

  return create_suboption_list_string(reportDescriptionCollection);
}

std::string 
XBUtilities::create_suboption_list_map(const std::string& deviceClass,
                                       const std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>>& device_options,
                                       const VectorPairStrings& common_options)
{
  std::map<std::string, VectorPairStrings> reportDescriptionCollection;

  VectorPairStrings commonCollection(common_options);
  bool printAllOptions = false;
  const auto& commonOptions = JSONConfigurable::extract_common_options(device_options);
  // If a device is specified only print applicable commands
  const auto it = device_options.find(deviceClass);
  if (it != device_options.end()) {
    for (const auto& option : it->second)
      commonCollection.emplace_back(option->getConfigName(), option->getConfigDescription());
  } else if (!device_options.empty())
    printAllOptions = true;

  if (printAllOptions) {
    for (const auto& option : commonOptions)
      commonCollection.emplace_back(option->getConfigName(), option->getConfigDescription());
  }

  reportDescriptionCollection.emplace(std::string("common"), commonCollection);

  if (printAllOptions) {
    // Add the device specific options
    for (const auto& report_pair : device_options) {
      VectorPairStrings collection;
      for (const auto& report : report_pair.second) {
        // Skip options that are common between device types
        const auto commonOptionsIt = commonOptions.find(report);
        if (commonOptionsIt != commonOptions.end())
          continue;

        // Skip hidden reports
        if (!XBU::getShowHidden() && report->getConfigHidden()) 
          continue;
        collection.emplace_back(report->getConfigName(), report->getConfigDescription());
      }

      auto map_it = JSONConfigurable::device_type_map.find(report_pair.first);
      if (map_it == JSONConfigurable::device_type_map.end())
        reportDescriptionCollection.emplace(report_pair.first, collection);
      else
        reportDescriptionCollection.emplace(map_it->second, collection);
    }
  }

  // Sort the collection
  for (auto& collection : reportDescriptionCollection) {
    sort(collection.second.begin(), collection.second.end(), 
        [](const std::pair<std::string, std::string> & a, const std::pair<std::string, std::string> & b) -> bool
        { return (a.first.compare(b.first) < 0); });
  }

  return create_suboption_list_map_string(reportDescriptionCollection);
}

std::string 
XBUtilities::create_suboption_list_string(const Report::SchemaDescriptionVector &_formatCollection)
{
  VectorPairStrings reportDescriptionCollection;

  // report names and description
  for (const auto & format : _formatCollection) {
    if (format.isVisable == true) 
      reportDescriptionCollection.emplace_back(format.optionName, format.shortDescription);
  }

  std::map<std::string, VectorPairStrings> collection = {
    {"common", reportDescriptionCollection}
  };
  return create_suboption_list_map_string(collection);
}

void 
XBUtilities::collect_and_validate_reports( const ReportCollection &allReportsAvailable,
                                           const std::vector<std::string> &reportNamesToAdd,
                                           ReportCollection & reportsToUse)
{
  // If "verbose" used, then use all of the reports
  if (std::find(reportNamesToAdd.begin(), reportNamesToAdd.end(), "all") != reportNamesToAdd.end()) {
    for (const auto & report : allReportsAvailable) {
      // Skip hidden reports
      if (report->isHidden()) 
        continue;
      reportsToUse.emplace_back(report);
    }
  } else { 
    // Examine each report name for a match 
    for (const auto & reportName : reportNamesToAdd) {
      auto iter = std::find_if(allReportsAvailable.begin(), allReportsAvailable.end(), 
                               [&reportName](const std::shared_ptr<Report>& obj) {return obj->getReportName() == boost::algorithm::to_lower_copy(reportName);});
      if (iter != allReportsAvailable.end()) 
        reportsToUse.push_back(*iter);
      else {
        throw xrt_core::error((boost::format("No report generator found for report: '%s'\n") % reportName).str());
      }
    }
  }
}


void 
XBUtilities::produce_reports( const std::shared_ptr<xrt_core::device>& device, 
                              const ReportCollection & reportsToProcess, 
                              const Report::SchemaVersion schemaVersion, 
                              const std::vector<std::string> & elementFilter,
                              std::ostream & consoleStream,
                              std::ostream & schemaStream)
{
  // Some simple DRCs
  if (reportsToProcess.empty()) {
    consoleStream << "Info: No action taken, no reports given.\n";
    return;
  }

  if (schemaVersion == Report::SchemaVersion::unknown) {
    consoleStream << "Info: No action taken, 'UNKNOWN' schema value specified.\n";
    return;
  }

  // Working property tree
  boost::property_tree::ptree ptRoot;

  // Add schema version
  {
    boost::property_tree::ptree ptSchemaVersion;
    ptSchemaVersion.put("schema", Report::getSchemaDescription(schemaVersion).optionName.c_str());
    ptSchemaVersion.put("creation_date", xrt_core::timestamp());

    ptRoot.add_child("schema_version", ptSchemaVersion);
  }

  bool is_report_output_valid = true;

  // -- Process the reports that don't require a device
  boost::property_tree::ptree ptSystem;
  for (const auto & report : reportsToProcess) {
    if (report->isDeviceRequired() == true)
      continue;

    boost::property_tree::ptree ptReport;
    try {
      report->getFormattedReport(nullptr, schemaVersion, elementFilter, consoleStream, ptReport);
    } catch (const std::exception&) {
      is_report_output_valid = false;
    }

    // Only support 1 node on the root
    if (ptReport.size() > 1)
      throw xrt_core::error((boost::format("Invalid JSON - The report '%s' has too many root nodes.") % Report::getSchemaDescription(schemaVersion).optionName).str());

    // We have 1 node, copy the child to the root property tree
    if (ptReport.size() == 1) {
      for (const auto & ptChild : ptReport) 
        ptSystem.add_child(ptChild.first, ptChild.second);
    }
  }
  if (!ptSystem.empty()) 
    ptRoot.add_child("system", ptSystem);

  // -- Check if any device specific report is requested
  auto dev_report = [reportsToProcess]() {
    for (auto &report : reportsToProcess) {
      if (report->isDeviceRequired() == true)
        return true;
    }
    return false;
  };

  if(dev_report()) {
    // -- Process reports that work on a device
    boost::property_tree::ptree ptDevice;
    auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);
    ptDevice.put("interface_type", "pcie");
    ptDevice.put("device_id", xrt_core::query::pcie_bdf::to_string(bdf));

    switch (xrt_core::device_query_default<xrt_core::query::device_class>(device, xrt_core::query::device_class::type::alveo)) {
    case xrt_core::query::device_class::type::alveo:
    {
      const auto device_status = xrt_core::device_query_default<xrt_core::query::device_status>(device, 2);
      ptDevice.put("device_status", xrt_core::query::device_status::parse_status(device_status));
      break;
    }
    case xrt_core::query::device_class::type::ryzen:
      // Ryzen devices do not have a concept of device status. They work or they dont.
      break;
    }

    bool is_mfg = false;
    try {
      is_mfg = xrt_core::device_query<xrt_core::query::is_mfg>(device);
    } 
    catch (const xrt_core::query::exception&) {
      is_mfg = false;
    }

    //if factory mode
    std::string platform;
    try {
      if (is_mfg) {
        platform = "xilinx_" + xrt_core::device_query<xrt_core::query::board_name>(device) + "_GOLDEN";
      }
      else {
        platform = xrt_core::device_query<xrt_core::query::rom_vbnv>(device);
      }
    } 
    catch (const xrt_core::query::exception&) {
      // proceed even if the platform name is not available
      platform = "<not defined>";
    }
    // Bound the device description on top and bottom with '-' characters
    const std::string dev_desc = (boost::format("[%s] : %s\n") % ptDevice.get<std::string>("device_id") % platform).str();
    consoleStream << std::endl;
    consoleStream << std::string(dev_desc.length(), '-') << std::endl;
    consoleStream << dev_desc;
    consoleStream << std::string(dev_desc.length(), '-') << std::endl;

    const auto is_ready = xrt_core::device_query_default<xrt_core::query::is_ready>(device, true);
    bool is_recovery = false;
    try {
      is_recovery = xrt_core::device_query<xrt_core::query::is_recovery>(device);
    }
    catch(const xrt_core::query::exception&) { 
      is_recovery = false;
    }

    // Process the tests that require a device
    // If the device is either of the following, most tests cannot be completed fully:
    // 1. Is in factory mode and is not in recovery mode
    // 2. Is not ready and is not in recovery mode
    if ((is_mfg || !is_ready) && !is_recovery)
      std::cout << "Warning: Device is not ready - Limited functionality available with XRT tools.\n";

    for (auto &report : reportsToProcess) {
      if (!report->isDeviceRequired())
        continue;

      boost::property_tree::ptree ptReport;
      try {
        report->getFormattedReport(device.get(), schemaVersion, elementFilter, consoleStream, ptReport);
      } catch (const std::exception&) {
        is_report_output_valid = false;
      }

      // Only support 1 node on the root
      if (ptReport.size() > 1)
        throw xrt_core::error((boost::format("Invalid JSON - The report '%s' has too many root nodes.") % Report::getSchemaDescription(schemaVersion).optionName).str());

      // We have 1 node, copy the child to the root property tree
      if (ptReport.size() == 1) {
        for (const auto & ptChild : ptReport)
          ptDevice.add_child(ptChild.first, ptChild.second);
      }
    }

    if (!ptDevice.empty()) {
      boost::property_tree::ptree ptDevices;
      ptDevices.push_back(std::make_pair("", ptDevice));
      ptRoot.add_child("devices", ptDevices);
    }
  }

  // -- Write the formatted output 
  switch (schemaVersion) {
    case Report::SchemaVersion::json_20202:
      boost::property_tree::json_parser::write_json(schemaStream, ptRoot, true /*Pretty Print*/);
      schemaStream << std::endl;  
      break;

    default:
      // Do nothing
      break;
  }

  // If any the data reports failed to generate with an exception throw an operation cancelled but output everything
  if(!is_report_output_valid)
    throw xrt_core::error(std::errc::operation_canceled);
}

