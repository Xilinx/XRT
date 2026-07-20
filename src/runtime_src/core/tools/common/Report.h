// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __Report_h_
#define __Report_h_

// Please keep eternal include file dependencies to a minimum
#include "core/common/device.h"
#include "JSONConfigurable.h"
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <string>
#include <vector>

class Report : public JSONConfigurable {
 public:
  // Supported JSON schemas.
  // Numbered ABIs are introduced only for parser-breaking ptree
  // changes. Alveo / AIE2 use --format (optionName). Following devices use --json
  // (jsonVersionName). "JSON" / "default" always track the newest ABI.
  enum class SchemaVersion  {
    unknown,
    json_internal,
    json_latest,
    json_20202,      // Legacy ABI (--format JSON-2020.2 only, identical to JSON)
  };
 
  struct SchemaDescription {
    SchemaVersion schemaVersion;
    bool isVisable;              // Listed in --format help when optionName is set
    std::string optionName;      // --format value (e.g. JSON, JSON-2020.2)
    std::string jsonVersionName; // --json value (e.g. default)
    std::string shortDescription;
  };

  using SchemaDescriptionVector = std::vector<SchemaDescription>;
  static const SchemaDescriptionVector m_schemaVersionVector;

  static const Report::SchemaDescription & getSchemaDescription(const std::string & _schemaVersionName);
  static const Report::SchemaDescription & getSchemaDescription(SchemaVersion _schemaVersion);
  static const SchemaDescriptionVector & getSchemaDescriptionVector() { return m_schemaVersionVector; };

  static SchemaVersion resolveSchemaFromJsonVersion(const std::string& jsonVersion);
  static SchemaVersion resolveSchemaFromFormatName(const std::string& formatName);
  static std::string getSchemaOutputLabel(SchemaVersion schemaVersion, bool useJsonVersionNaming);

  struct JsonSchemaSelection {
    SchemaVersion schemaVersion;
    bool useJsonVersionNaming;
  };

  static JsonSchemaSelection selectJsonSchema(bool hasJsonOption,
                                              const std::string& jsonVersion,
                                              bool hasFormatOption,
                                              const std::string& formatName,
                                              const std::shared_ptr<xrt_core::device>& device);

  // Supporting APIs
 public:
  const std::string & getReportName() const { return m_reportName; };
  const std::string & getConfigName() const { return getReportName(); };
  const std::string & getShortDescription() const { return m_shortDescription; };
  const std::string &getConfigDescription() const { return getShortDescription(); };
  bool isDeviceRequired() const { return m_isDeviceRequired; };
  bool isHidden() const { return m_isHidden; };
  bool getConfigHidden() const {return isHidden();};

  void getFormattedReport(const xrt_core::device *_pDevice, SchemaVersion _schemaVersion, const std::vector<std::string> & _elementFilter, std::ostream & consoleStream, boost::property_tree::ptree & pt) const;

 // Needs a virtual destructor
  virtual ~Report() {};

 // Child methods that need to be implemented
 protected:
  virtual void writeReport(const xrt_core::device* _pDevice, const boost::property_tree::ptree& pt, const std::vector<std::string>& _elementsFilter,std::ostream & _output) const = 0;
  virtual void getPropertyTreeInternal(const xrt_core::device *_pDevice, boost::property_tree::ptree &_pt) const = 0;
  virtual void getPropertyTree20202(const xrt_core::device *_pDevice, boost::property_tree::ptree &_pt) const = 0;

 // Child class Helper methods
 protected:
  Report(const std::string & _reportName, const std::string & _shortDescription, bool _deviceRequired);
  Report(const std::string & _reportName, const std::string & _shortDescription, bool _deviceRequired, bool _isHidden);

 private:
  Report() = delete;

 // Variables
 private:
   std::string m_reportName;
   std::string m_shortDescription;
   bool m_isDeviceRequired;

   bool m_isHidden;
};


// -- Helper collection
using ReportCollection = std::vector<std::shared_ptr<Report>>;

#endif

