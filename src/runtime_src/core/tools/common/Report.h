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
  // Numbered ABIs are for parser-breaking ptree changes.
  // Alveo / AIE2 use --format, else use --json.
  enum class SchemaVersion  {
    unknown,
    json_internal,
    json_latest,
    json_20202,      // Legacy ABI (--format JSON-2020.2 only, identical to JSON)
  };

  struct SchemaDescription {
    SchemaVersion schemaVersion;
    bool isVisable;                // Listed in --format help
    std::string optionName;
    std::string shortDescription;
  };

  using SchemaDescriptionVector = std::vector<SchemaDescription>;
  static const SchemaDescriptionVector m_schemaVersionVector;

  static const Report::SchemaDescription & getSchemaDescription(const std::string & _schemaVersionName);
  static const Report::SchemaDescription & getSchemaDescription(SchemaVersion _schemaVersion);
  static const SchemaDescriptionVector & getSchemaDescriptionVector() { return m_schemaVersionVector; };

  /**
   * Resolves the JSON ABI from CLI state.
   * @param json_platform     true when --json is active (explicit or platform default)
   * @param explicit_selector true when the user passed --json or --format
   * @param version           --json or --format value depending on json_platform
   */
  static SchemaVersion resolve_json_abi(bool json_platform,
                                        bool explicit_selector,
                                        const std::string& version);

  /** Helpers for JSON file output (header node, ABI-specific tree shaping). */
  struct JsonAbi {
    /** True for ABIs that write user-facing JSON to -o (excludes internal/unknown). */
    static bool valid_user_abi(SchemaVersion version);
    static boost::property_tree::ptree make_json_header(const std::string& schema_label);
    static boost::property_tree::ptree fit_abi_tree(SchemaVersion version,
                                                    const boost::property_tree::ptree& tree);
  };

  // Supporting APIs
 public:
  const std::string & getReportName() const { return m_reportName; };
  const std::string & getConfigName() const { return getReportName(); };
  const std::string & getShortDescription() const { return m_shortDescription; };
  const std::string &getConfigDescription() const { return getShortDescription(); };
  bool isDeviceRequired() const { return m_isDeviceRequired; };
  bool isHidden() const { return m_isHidden; };
  bool getConfigHidden() const {return isHidden();};

  virtual bool clearScreenBeforeReports() const { return false; }

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
