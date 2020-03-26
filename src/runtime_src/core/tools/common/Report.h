/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef __Report_h_
#define __Report_h_

// Please keep eternal include file dependencies to a minimum
#include "core/common/device.h"
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <string>
#include <vector>

class Report {
 public:
  // Supported JSON schemas.
  // 
  // Remember to update the initialization of Report::m_schemaVersionMapping 
  // if new enumeration values are added
  enum class SchemaVersion  {
    text,
    unknown,
    json_internal,
    json_20201,
  };
 
  // Helper mapping between string and enum
  struct SchemaDescription {
    SchemaVersion schemaVersion;
    bool isVisable;
    std::string optionName;
    std::string shortDescription;
  };

  using SchemaDescriptionVector = std::vector<SchemaDescription>;
  static const SchemaDescriptionVector m_schemaVersionVector;

  static const Report::SchemaDescription & getSchemaDescription(const std::string & _schemaVersionName);
  static const Report::SchemaDescription & getSchemaDescription(SchemaVersion _schemaVersion);
  static const SchemaDescriptionVector & getSchemaDescriptionVector() { return m_schemaVersionVector; };

  // Supporting APIs
 public:
  const std::string & getReportName() const { return m_reportName; };
  const std::string & getShortDescription() const { return m_shortDescription; };
  bool isDeviceRequired() const { return m_isDeviceRequired; };

  boost::any getFormattedReport(const xrt_core::device *_pDevice, SchemaVersion _schemaVersion, const std::vector<std::string> & _elementFilter) const;

 // Child methods that need to be implemented
 protected:
  virtual void writeReport(const xrt_core::device *_pDevice, const std::vector<std::string> & _elementsFilter, std::iostream & _output) const = 0;
  virtual void getPropertyTreeInternal(const xrt_core::device *_pDevice, boost::property_tree::ptree &_pt) const = 0;
  virtual void getPropertyTree20201(const xrt_core::device *_pDevice, boost::property_tree::ptree &_pt) const = 0;

 // Child class Helper methods
 protected:
  Report(const std::string & _reportName, const std::string & _shortDescription, bool _deviceRequired);

 private:
  Report() = delete;

 // Variables
 private:
   std::string m_reportName;
   std::string m_shortDescription;
   bool m_isDeviceRequired;
};


// -- Helper collection
using ReportCollection = std::vector<std::shared_ptr<Report>>;

#endif


