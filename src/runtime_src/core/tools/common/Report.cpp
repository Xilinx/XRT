/**
 * Copyright (C)2020 Xilinx, Inc
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
#include <sstream>

// Initialize our static mapping.
const Report::SchemaDescriptionVector Report::m_schemaVersionVector = {
  { SchemaVersion::unknown,       false, "",              "Unknown entry"},
  { SchemaVersion::text,          true,  "text",          "Human readable report (default)"},
  { SchemaVersion::json_20202,    true,  "JSON",          "Latest JSON schema"}, // Note: to be updated to the latest schema version every release
  { SchemaVersion::json_internal, false, "JSON-internal", "Internal JSON property tree"},
  { SchemaVersion::json_20202,    true,  "JSON-2020.2",   "JSON 2020.2 schema"}
};


const Report::SchemaDescription &
Report::getSchemaDescription(const std::string & _schemaVersionName) 
{
  // Look for a match
  for (const auto & entry : m_schemaVersionVector) {
    if (boost::iequals(entry.optionName, _schemaVersionName)) 
      return entry;
  }

  // Return back the unknown entry
  return getSchemaDescription("");
}

const Report::SchemaDescription & 
Report::getSchemaDescription(SchemaVersion _schemaVersion)
{
  // Look for a match
  for (const auto & entry : m_schemaVersionVector) {
    if (entry.schemaVersion == _schemaVersion) 
      return entry;
  }

  // Return back the unknown entry
  return getSchemaDescription(SchemaVersion::unknown);
}


Report::Report(const std::string & _reportName,
               const std::string & _shortDescription,
               bool _isDeviceRequired)
  : m_reportName(_reportName)
  , m_shortDescription(_shortDescription)
  , m_isDeviceRequired(_isDeviceRequired)
{
  // Empty
}


boost::any 
Report::getFormattedReport(const xrt_core::device *_pDevice, 
                           SchemaVersion _schemaVersion,
                           const std::vector<std::string> & _elementFilter) const
{
  // Helper variables
  boost::property_tree::ptree _pt;
  std::stringstream _ostream;
  boost::any returnValue;

  switch (_schemaVersion) {
    case SchemaVersion::text:  
      writeReport(_pDevice, _elementFilter, _ostream);
      returnValue = _ostream.str();
      return returnValue;

    case SchemaVersion::json_internal:
      getPropertyTreeInternal(_pDevice, _pt);
      returnValue = _pt;
      return returnValue;

    case SchemaVersion::json_20202:
      getPropertyTree20202(_pDevice, _pt);
      returnValue = _pt;
      return returnValue;

    case SchemaVersion::unknown:
      throw std::runtime_error("ERROR: Unknown schema version.");

    // Note: There is no default in this switch statement.  Relying on the
    //       compiler to produce a warning indicating that all of the enum values
    //       are present.
  }

  // Code will never get here, but the compiler doesn't know that.
  throw std::runtime_error("Report::getFormattedReport() - Unexpected execution in the code flow.");
}

