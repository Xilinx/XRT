// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

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
  // Nagios data area
  enum class NagiosStatus {
    okay = 0,
    warning,
    critical,
    unknown
  };

  // Define these operators to allow fo easy comparison of device states
  friend bool operator<(NagiosStatus rhs, NagiosStatus lhs)
  {
    int val_rhs = static_cast<int>(rhs);
    int val_lhs = static_cast<int>(lhs);
    return val_rhs < val_lhs;
  }

  friend bool operator>(NagiosStatus rhs, NagiosStatus lhs)
  {
    int val_rhs = static_cast<int>(rhs);
    int val_lhs = static_cast<int>(lhs);
    return val_rhs > val_lhs;
  }

  // Supported JSON schemas.
  // 
  // Remember to update the initialization of Report::m_schemaVersionMapping 
  // if new enumeration values are added
  enum class SchemaVersion  {
    unknown,
    json_internal,
    json_20202,
    json_plain
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
  bool isHidden() const { return m_isHidden; };

  void getFormattedReport(const xrt_core::device *_pDevice, SchemaVersion _schemaVersion, const std::vector<std::string> & _elementFilter, std::ostream & consoleStream, boost::property_tree::ptree & pt) const;

 // Needs a virtual destructor
  virtual ~Report() {};

 // Child methods that need to be implemented
 protected:
  virtual void writeReport(const xrt_core::device* _pDevice, const boost::property_tree::ptree& pt, const std::vector<std::string>& _elementsFilter,std::ostream & _output) const = 0;
  virtual void getPropertyTreeInternal(const xrt_core::device *_pDevice, boost::property_tree::ptree &_pt) const = 0;
  virtual void getPropertyTree20202(const xrt_core::device *_pDevice, boost::property_tree::ptree &_pt) const = 0;

// Formatting constant for Nagios plugin
protected:
  static boost::format m_nagiosFormat;

 // Child class Helper methods
 protected:
  Report(const std::string & _reportName, const std::string & _shortDescription, bool _deviceRequired);
  Report(const std::string & _reportName, const std::string & _shortDescription, bool _deviceRequired, bool _isHidden);

 private:
  void populatePropertyTree( const xrt_core::device *pDevice, SchemaVersion schemaVersion, boost::property_tree::ptree & pt) const;
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


