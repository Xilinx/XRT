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
  { SchemaVersion::unknown,       false, "",            "Unknown entry"},
  { SchemaVersion::json_latest,   true,  "JSON",        "Latest JSON schema (default)"},
  { SchemaVersion::json_latest,   false, "default",     "Latest JSON schema (default)"},
  { SchemaVersion::json_internal, false, "",            "Internal JSON property tree"},
  { SchemaVersion::json_20202,    true,  "JSON-2020.2", "JSON 2020.2 schema (legacy)"},
};

const Report::SchemaDescription &
Report::getSchemaDescription(const std::string & name)
{
  for (const auto & entry : m_schemaVersionVector) {
    if (!entry.optionName.empty() && boost::iequals(entry.optionName, name))
      return entry;
  }

  return getSchemaDescription(SchemaVersion::unknown);
}

const Report::SchemaDescription &
Report::getSchemaDescription(SchemaVersion version)
{
  // A SchemaVersion may map to multiple registry rows (e.g. json_latest has both
  // "JSON" and "default"). Prefer the --format row for canonical naming in errors.
  const SchemaDescription* fallback = nullptr;
  for (const auto & entry : m_schemaVersionVector) {
    if (entry.schemaVersion != version)
      continue;
    if (entry.isVisable)
      return entry;
    fallback = &entry;
  }

  // No --format row (e.g. json_internal); return the sole --json or internal row.
  if (fallback)
    return *fallback;

  return getSchemaDescription(SchemaVersion::unknown);
}

Report::SchemaVersion
Report::resolve_json_abi(bool json_platform, bool explicit_selector, const std::string& version)
{
  if (!explicit_selector)
    return SchemaVersion::json_latest;

  // json_platform selects isVisable=false (--json) vs isVisable=true (--format) entries.
  // Each path rejects names that belong to the other flag.
  const auto lookup_name = json_platform && version.empty() ? "default" : version;

  for (const auto& entry : m_schemaVersionVector) {
    if (entry.optionName.empty() || !boost::iequals(entry.optionName, lookup_name))
      continue;
    if (entry.isVisable == !json_platform)
      return entry.schemaVersion;
    return SchemaVersion::unknown;
  }

  return SchemaVersion::unknown;
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
Report::JsonAbi::make_json_header(const std::string& schema_label)
{
  boost::property_tree::ptree node;
  node.put("schema", schema_label.empty() ? "unknown" : schema_label);
  node.put("creation_date", xrt_core::timestamp());
  return node;
}

// Shapes the internal report tree to match a frozen JSON ABI.
boost::property_tree::ptree
Report::JsonAbi::fit_abi_tree(SchemaVersion /*version*/,
                              const boost::property_tree::ptree& tree)
{
  return tree;
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
