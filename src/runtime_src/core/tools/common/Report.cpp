/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2020-2022 Xilinx, Inc
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
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
#include "Report.h"
#include "ReportSchemaProjector.h"
#include "tools/common/XBUtilities.h"

#include "core/common/query_requests.h"
#include "core/common/smi/smi.h"

#include <boost/algorithm/string/predicate.hpp>

namespace xq = xrt_core::query;

// Initialize our static mapping.
const Report::SchemaDescriptionVector Report::m_schemaVersionVector = {
  { SchemaVersion::unknown,       false, "",              "",        "Unknown entry"},
  { SchemaVersion::json_latest,   true,  "JSON",          "default", "Latest JSON schema (default)"},
  { SchemaVersion::json_internal, false, "",              "",        "Internal JSON property tree"},
  { SchemaVersion::json_20202,    true,  "JSON-2020.2",   "",        "JSON 2020.2 schema (legacy)"},
};


const Report::SchemaDescription &
Report::getSchemaDescription(const std::string & _schemaVersionName) 
{
  for (const auto & entry : m_schemaVersionVector) {
    if (!entry.optionName.empty() && boost::iequals(entry.optionName, _schemaVersionName))
      return entry;
    if (!entry.jsonVersionName.empty() && boost::iequals(entry.jsonVersionName, _schemaVersionName))
      return entry;
  }

  return getSchemaDescription("");
}

const Report::SchemaDescription & 
Report::getSchemaDescription(SchemaVersion _schemaVersion)
{
  for (const auto & entry : m_schemaVersionVector) {
    if (entry.schemaVersion == _schemaVersion) 
      return entry;
  }

  return getSchemaDescription(SchemaVersion::unknown);
}

Report::SchemaVersion
Report::resolveSchemaFromJsonVersion(const std::string& jsonVersion)
{
  if (jsonVersion.empty() || boost::iequals(jsonVersion, "default"))
    return SchemaVersion::json_latest;

  for (const auto& entry : m_schemaVersionVector) {
    if (!entry.jsonVersionName.empty()
        && boost::iequals(entry.jsonVersionName, jsonVersion))
      return entry.schemaVersion;
  }

  return SchemaVersion::unknown;
}

Report::SchemaVersion
Report::resolveSchemaFromFormatName(const std::string& formatName)
{
  for (const auto& entry : m_schemaVersionVector) {
    if (!entry.optionName.empty()
        && boost::iequals(entry.optionName, formatName))
      return entry.schemaVersion;
  }

  return SchemaVersion::unknown;
}

std::string
Report::getSchemaOutputLabel(SchemaVersion schemaVersion, bool useJsonVersionNaming)
{
  const auto& desc = getSchemaDescription(schemaVersion);
  if (useJsonVersionNaming && !desc.jsonVersionName.empty())
    return desc.jsonVersionName;
  if (!desc.optionName.empty())
    return desc.optionName;
  return "unknown";
}

Report::JsonSchemaSelection
Report::selectJsonSchema(bool hasJsonOption,
                         const std::string& jsonVersion,
                         bool hasFormatOption,
                         const std::string& formatName,
                         const std::shared_ptr<xrt_core::device>& device)
{
  if (hasJsonOption) {
    const std::string jsonVer = jsonVersion.empty() ? "default" : jsonVersion;
    return { resolveSchemaFromJsonVersion(jsonVer), true };
  }

  if (hasFormatOption)
    return { resolveSchemaFromFormatName(formatName), false };

  bool useJsonAbi = false;
  if (device) {
    try {
      useJsonAbi = xq::device_query<xq::device_class>(device.get()) == xq::device_class::type::ryzen
        && !XBUtilities::is_strix_hardware(
             xrt_core::smi::smi_hardware_config{}.get_hardware_type(xq::device_query<xq::pcie_id>(device)));
    } catch (const std::exception&) {
      try {
        useJsonAbi = xq::device_query<xq::device_class>(device.get()) == xq::device_class::type::ryzen;
      } catch (const std::exception&) {}
    }
  }

  return { SchemaVersion::json_latest, useJsonAbi };
}

Report::Report(const std::string & _reportName,
               const std::string & _shortDescription,
               bool _isDeviceRequired)
  : m_reportName(_reportName)
  , m_shortDescription(_shortDescription)
  , m_isDeviceRequired(_isDeviceRequired)
  , m_isHidden(false)
{
  // Empty
}

Report::Report(const std::string & _reportName,
               const std::string & _shortDescription,
               bool _isDeviceRequired,
               bool _isHidden)
  : m_reportName(_reportName)
  , m_shortDescription(_shortDescription)
  , m_isDeviceRequired(_isDeviceRequired)
  , m_isHidden(_isHidden)
{
  // Empty
}

void 
Report::getFormattedReport( const xrt_core::device *pDevice, 
                            SchemaVersion schemaVersion,
                            const std::vector<std::string> & elementFilter,
                            std::ostream & consoleStream,
                            boost::property_tree::ptree & pt) const
{
  try {
    switch (schemaVersion) {
      case SchemaVersion::json_internal:
        getPropertyTreeInternal(pDevice, pt);
        break;

      case SchemaVersion::json_latest:
      case SchemaVersion::json_20202: {
        boost::property_tree::ptree superset;
        getPropertyTreeInternal(pDevice, superset);
        pt = ReportSchemaProjector::project(schemaVersion, superset);
        break;
      }

      default:
        throw std::runtime_error("ERROR: Unknown schema version.");
        break;
    }

    writeReport(pDevice, pt, elementFilter, consoleStream);
  } catch (const std::exception& e) {
    std::string reportName = getReportName();
    if (!reportName.empty()) {
      reportName[0] = static_cast<char>(std::toupper(reportName[0]));
      std::cerr << reportName << std::endl;
    }

    std::cerr << "  ERROR: " << e.what() << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }
}
