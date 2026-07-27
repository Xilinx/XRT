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

#include "core/common/time.h"
#include <boost/algorithm/string/predicate.hpp>

// Initialize our static mapping.
const Report::SchemaDescriptionVector Report::m_schemaVersionVector = {
  { SchemaVersion::unknown,       false, "",            "",        "Unknown entry"},
  { SchemaVersion::json_latest,   true,  "JSON",        "default", "Latest JSON schema (default)"},
  { SchemaVersion::json_internal, false, "",            "",        "Internal JSON property tree"},
  { SchemaVersion::json_20202,    true,  "JSON-2020.2", "",        "JSON 2020.2 schema (legacy)"},
};

namespace {

// Maps a CLI version string to SchemaVersion.
// for_json=true matches json_version_name (--json); false matches optionName (--format).
// Each path rejects names that belong to the other flag.
Report::SchemaVersion
lookup_abi(const std::string& name, bool for_json)
{
  if (for_json) {
    if (name.empty() || boost::iequals(name, "default"))
      return Report::SchemaVersion::json_latest;

    for (const auto& entry : Report::m_schemaVersionVector) {
      if (!entry.json_version_name.empty()
          && boost::iequals(entry.json_version_name, name))
        return entry.schemaVersion;
    }
    return Report::SchemaVersion::unknown;
  }

  for (const auto& entry : Report::m_schemaVersionVector) {
    if (!entry.optionName.empty() && boost::iequals(entry.optionName, name))
      return entry.schemaVersion;
  }

  // Reject --json-only names on the --format path (e.g. default).
  for (const auto& entry : Report::m_schemaVersionVector) {
    if (!entry.json_version_name.empty() && boost::iequals(entry.json_version_name, name))
      return Report::SchemaVersion::unknown;
  }

  return Report::SchemaVersion::unknown;
}

} // namespace

const Report::SchemaDescription &
Report::getSchemaDescription(const std::string & name)
{
  for (const auto & entry : m_schemaVersionVector) {
    if (!entry.optionName.empty() && boost::iequals(entry.optionName, name))
      return entry;
    if (!entry.json_version_name.empty() && boost::iequals(entry.json_version_name, name))
      return entry;
  }

  return getSchemaDescription(SchemaVersion::unknown);
}

const Report::SchemaDescription &
Report::getSchemaDescription(SchemaVersion version)
{
  for (const auto & entry : m_schemaVersionVector) {
    if (entry.schemaVersion == version)
      return entry;
  }

  return getSchemaDescription(SchemaVersion::unknown);
}

Report::JsonAbiChoice
Report::resolve_json_abi(bool json_platform, bool explicit_selector, const std::string& version)
{
  if (json_platform)
    return { lookup_abi(version.empty() ? "default" : version, true), true };

  if (explicit_selector)
    return { lookup_abi(version, false), false };

  return { SchemaVersion::json_latest, false };
}

bool
Report::JsonAbi::valid_user_abi(SchemaVersion version)
{
  switch (version) {
  case SchemaVersion::json_latest:
  case SchemaVersion::json_20202:
    return true;
  default:
    return false;
  }
}

boost::property_tree::ptree
Report::JsonAbi::make_json_header(SchemaVersion version, bool use_json_name)
{
  const auto& desc = getSchemaDescription(version);
  const std::string& label = use_json_name && !desc.json_version_name.empty()
                           ? desc.json_version_name
                           : desc.optionName;

  boost::property_tree::ptree node;
  node.put("schema", label.empty() ? "unknown" : label);
  node.put("creation_date", xrt_core::timestamp());
  return node;
}

// Shapes the internal report tree to match a frozen JSON ABI.
// Add a case when a new ABI removes or renames nodes from the internal superset.
boost::property_tree::ptree
Report::JsonAbi::fit_abi_tree(SchemaVersion version, const boost::property_tree::ptree& tree)
{
  switch (version) {
  case SchemaVersion::json_latest:
  case SchemaVersion::json_20202:
    return tree;
  default:
    return tree;
  }
}

Report::Report(const std::string & _reportName,
               const std::string & _shortDescription,
               bool _isDeviceRequired)
  : m_reportName(_reportName)
  , m_shortDescription(_shortDescription)
  , m_isDeviceRequired(_isDeviceRequired)
  , m_isHidden(false)
{
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
}

void
Report::getFormattedReport(const xrt_core::device *pDevice,
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
        boost::property_tree::ptree internal;
        getPropertyTreeInternal(pDevice, internal);
        pt = JsonAbi::fit_abi_tree(schemaVersion, internal);
        break;
      }

      default:
        throw std::runtime_error("ERROR: Unknown schema version.");
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
